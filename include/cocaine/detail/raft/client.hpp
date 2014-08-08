/*
    Copyright (c) 2014-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_RAFT_CLIENT_HPP
#define COCAINE_RAFT_CLIENT_HPP

#include "cocaine/detail/raft/forwards.hpp"

#include "cocaine/detail/client.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/context.hpp"
#include "cocaine/tuple.hpp"

namespace cocaine { namespace raft {

namespace detail {

template<class Drain>
struct event_result { };

template<class T>
struct event_result<cocaine::io::streaming_tag<T>> {
    typedef typename cocaine::tuple::fold<T>::type tuple_type;
    typedef typename std::tuple_element<0, tuple_type>::type type;
};

template<class... Args>
struct event_result<cocaine::io::streaming_tag<std::tuple<Args...>>> {
    typedef std::tuple<Args...> tuple_type;
    typedef typename std::tuple_element<0, tuple_type>::type type;
};

} // namespace detail

class disposable_client_t {
public:
    disposable_client_t(
        context_t &context,
        io::reactor_t &reactor,
        const std::string& name,
        const std::vector<node_id_t>& remotes,
        // Probably there is no sense to do more then 2 jumps,
        // because usually most of nodes know where is leader.
        unsigned int follow_redirect = 2,
        // Boost optional is needed to provide default value dependent on the context.
        boost::optional<float> request_timeout = boost::none
    ):
        m_reactor(reactor),
        m_logger(context.log("raft_client/" + name)),
        m_timeout_timer(reactor.native()),
        m_retry_timer(reactor.native()),
        m_name(name),
        m_follow_redirect(follow_redirect),
        m_request_timeout(2.0),
        m_remotes(remotes),
        m_next_remote(0),
        m_redirection_counter(0)
    {
        if(request_timeout) {
            m_request_timeout = *request_timeout;
        } else {
            // Currently this client is used to configuration changes.
            // One configuration change requires 5-7 roundtrips,
            // so 6 election timeouts is a reasonable value for configuration change timeout.
            m_request_timeout = float(6 * context.raft().options().election_timeout) / 1000.0;
        }

        m_timeout_timer.set<disposable_client_t, &disposable_client_t::on_timeout>(this);
        m_retry_timer.set<disposable_client_t, &disposable_client_t::resend_request>(this);
    }

    ~disposable_client_t() {
        reset();
    }

    template<class Event, class ResultHandler, class ErrorHandler, class... Args>
    void
    call(const ResultHandler& result_handler,
         const ErrorHandler& error_handler,
         Args&&... args)
    {
        typedef typename detail::event_result<typename io::event_traits<Event>::upstream_type>::type
                result_type;

        m_error_handler = error_handler;

        m_request_sender = std::bind(&disposable_client_t::send_request<Event, result_type>,
                                     this,
                                     std::function<void(const result_type&)>(result_handler),
                                     io::aux::make_frozen<Event>(std::forward<Args>(args)...));

        try_next_remote();
    }

private:
    void
    reset() {
        reset_request();

        m_posted_task.reset();
        m_client.reset();
        m_resolver.reset();
    }

    void
    reset_request() {
        if(m_timeout_timer.is_active()) {
            m_timeout_timer.stop();
        }

        if(m_retry_timer.is_active()) {
            m_retry_timer.stop();
        }

        if(m_current_request) {
            m_current_request->revoke();
            m_current_request.reset();
        }
    }

    template<class Event>
    struct client_caller {
        typedef std::shared_ptr<io::basic_upstream_t> result_type;

        const std::shared_ptr<cocaine::client_t>& m_client;
        const std::shared_ptr<io::basic_dispatch_t>& m_handler;

        template<class... Args>
        std::shared_ptr<io::basic_upstream_t>
        operator()(Args&&... args) const {
            return m_client->call<Event>(m_handler, std::forward<Args>(args)...);
        }
    };

    template<class Event, class Result>
    void
    send_request(const std::function<void(const Result&)>& handler,
                 const io::aux::frozen<Event>& event)
    {
        using namespace std::placeholders;

        auto dispatch = make_proxy<Result>(
            std::bind(&disposable_client_t::on_response<Result>, this, handler, _1)
        );

        m_current_request = tuple::invoke(client_caller<Event> {m_client, dispatch}, event.tuple);
    }

    template<class CommandResult>
    void
    on_response(const std::function<void(const CommandResult&)>& callback,
                const boost::variant<std::error_code, CommandResult>& response)
    {
        reset_request();

        if(boost::get<CommandResult>(&response)) {
            const auto &result = boost::get<CommandResult>(response);

            auto ec = result.error();

            if(!ec) {
                // Entry has been successfully committed to the state machine.
                std::function<void()> handler = std::bind(callback, result);

                auto callback_ptr = std::make_shared<std::function<void()>>(
                    std::bind(&disposable_client_t::reset_then, this, handler)
                );

                m_posted_task = callback_ptr;

                m_reactor.post(io::make_task(callback_ptr));
            } else if(ec == raft_errc::not_leader || ec == raft_errc::unknown) {
                // Connect to leader received from the remote or try the next remote.

                std::function<void()> connector;

                if(m_redirection_counter < m_follow_redirect && result.leader() != node_id_t()) {
                    ++m_redirection_counter;

                    connector = std::bind(&disposable_client_t::ensure_connection,
                                          this,
                                          result.leader());
                } else {
                    connector = std::bind(&disposable_client_t::try_next_remote, this);
                }

                auto resetter = std::make_shared<std::function<void()>>(
                    std::bind(&disposable_client_t::reset_then, this, connector)
                );

                m_posted_task = resetter;

                m_reactor.post(io::make_task(resetter));
            } else {
                // The state machine is busy (some configuration change is in progress).
                // Retry after request timeout.
                m_retry_timer.start(m_request_timeout);
            }
        } else {
            // Some error has occurred. Try the next host.
            std::function<void()> connector = std::bind(&disposable_client_t::try_next_remote, this);

            auto resetter = std::make_shared<std::function<void()>>(
                std::bind(&disposable_client_t::reset_then, this, connector)
            );

            m_posted_task = resetter;

            m_reactor.post(io::make_task(resetter));
        }
    }

    void
    reset_then(const std::function<void()>& continuation) {
        reset();
        continuation();
    }

    void
    try_next_remote() {
        if(m_next_remote >= m_remotes.size()) {
            reset();
            auto error_handler = m_error_handler;
            error_handler();
            return;
        }

        const auto &endpoint = m_remotes[m_next_remote];

        if(!m_client) {
            m_next_remote = m_next_remote + 1;
        }

        m_redirection_counter = 0;

        ensure_connection(endpoint);
    }

    void
    ensure_connection(const node_id_t& endpoint) {
        if(m_current_request || m_resolver) {
            return;
        }

        m_timeout_timer.start(m_request_timeout);

        if(m_client) {
            m_request_sender();
        } else {
            COCAINE_LOG_DEBUG(m_logger,
                              "raft client is not connected, connecting to %s:%d",
                              endpoint.first,
                              endpoint.second)
            (blackhole::attribute::list({
                {"host", endpoint.first},
                {"port", endpoint.second}
            }));

            m_resolver.reset(new service_resolver_t(
                m_reactor,
                io::resolver<io::tcp>::query(endpoint.first, endpoint.second),
                m_name
            ));

            using namespace std::placeholders;

            m_resolver->bind(std::bind(&disposable_client_t::on_client_connected, this, _1),
                             std::bind(&disposable_client_t::on_error, this, _1));
        }
    }

    void
    on_client_connected(const std::shared_ptr<cocaine::client_t>& client) {
        COCAINE_LOG_DEBUG(m_logger, "client connected");
        m_resolver.reset();

        m_client = client;
        m_client->bind(std::bind(&disposable_client_t::on_error, this, std::placeholders::_1));

        m_request_sender();
    }

    void
    on_error(const std::error_code& ec) {
        COCAINE_LOG_DEBUG(m_logger, "connection error: [%d] %s", ec.value(), ec.message())
        (blackhole::attribute::list({
            {"error_code", ec.value()},
            {"error_message", ec.message()}
        }));

        reset();
        try_next_remote();
    }

    void
    on_timeout(ev::timer&, int) {
        reset();

        auto reconnector = std::make_shared<std::function<void()>>(
            std::bind(&disposable_client_t::try_next_remote, this)
        );

        m_posted_task = reconnector;

        m_reactor.post(io::make_task(reconnector));
    }

    void
    resend_request(ev::timer&, int) {
        reset_request();
        m_request_sender();
    }

private:
    io::reactor_t &m_reactor;

    const std::unique_ptr<logging::log_t> m_logger;

    // This timer resets connection and tries the next remote
    // if response from current remote was not received in m_request_timeout seconds.
    ev::timer m_timeout_timer;

    // This timer resends request to current remote after m_request_timeout seconds
    // if the remote was busy.
    ev::timer m_retry_timer;

    // Name of service.
    std::string m_name;

    // How many times the client should try a leader received from current remote.
    unsigned int m_follow_redirect;

    float m_request_timeout;

    // Remote nodes, which will be used to find a leader.
    std::vector<node_id_t> m_remotes;

    // Next remote to use.
    size_t m_next_remote;

    // How many times the client already followed to leader received from remote.
    // When it becomes equal to m_follow_redirect, the client resets it it zero and tries the next
    // node from m_remotes.
    unsigned int m_redirection_counter;

    std::function<void()> m_error_handler;

    // Current operation. It's stored until it becomes committed to replicated state machine.
    std::function<void()> m_request_sender;

    std::shared_ptr<void> m_posted_task;

    std::shared_ptr<cocaine::client_t> m_client;

    std::unique_ptr<service_resolver_t> m_resolver;

    std::shared_ptr<io::basic_upstream_t> m_current_request;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_CLIENT_HPP
