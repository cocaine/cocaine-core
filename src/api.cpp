/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/api/connect.hpp"
#include "cocaine/api/resolve.hpp"
#include "cocaine/api/storage.hpp"

#include "cocaine/rpc/asio/channel.hpp"

#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/vector.hpp"

#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_stream.hpp>
#include <boost/spirit/include/karma_string.hpp>

#include <asio/connect.hpp>

using namespace asio;
using namespace asio::ip;

// Connect

namespace cocaine { namespace api { namespace details {

basic_client_t::basic_client_t(basic_client_t&& other) {
    *this = std::move(other);
}

basic_client_t::~basic_client_t() {
    if(m_session) {
        m_session->detach();
    }
}

basic_client_t&
basic_client_t::operator=(basic_client_t&& rhs) {
    if(m_session && m_session != rhs.m_session) {
        m_session->detach();
    }

    if((m_session = std::move(rhs.m_session)) == nullptr) {
        return *this;
    }

    m_session_signals = m_session->signals.shutdown.connect(std::bind(&basic_client_t::cleanup,
        this,
        std::placeholders::_1
    ));

    // Unsubscribe the other client from session signals.
    rhs.m_session_signals.disconnect();

    return *this;
}

boost::optional<const session_t&>
basic_client_t::session() const {
    return boost::optional<const session_t&>(m_session != nullptr, *m_session);
}

void
basic_client_t::connect(std::unique_ptr<tcp::socket> socket) {
    if(m_session) {
        throw cocaine::error_t("client is already connected");
    }

    m_session = std::make_shared<session_t>(
        std::make_unique<io::channel<tcp>>(std::move(socket)),
        nullptr
    );

    m_session_signals = m_session->signals.shutdown.connect(std::bind(&basic_client_t::cleanup,
        this,
        std::placeholders::_1
    ));

    m_session->pull();
}

void
basic_client_t::cleanup(const std::error_code& COCAINE_UNUSED_(ec)) {
    if(m_session) {
        m_session->detach();
        m_session = nullptr;
    }
}

}}} // namespace cocaine::api::details

using namespace cocaine::api;
using namespace cocaine::api::details;

// Resolve internals

class resolve_t::resolve_action_t:
    public dispatch<io::event_traits<io::locator::resolve>::upstream_type>
{
    resolve_t *const parent;
    basic_client_t&  client;

    // User-supplied completion handler.
    handler_type handle;

public:
    resolve_action_t(resolve_t *const parent_, basic_client_t& client_, handler_type handle_):
        dispatch<io::event_traits<io::locator::resolve>::upstream_type>("resolve"),
        parent(parent_),
        client(client_),
        handle(handle_)
    {
        typedef io::protocol<io::event_traits<io::locator::resolve>::upstream_type>::scope protocol;

        using namespace std::placeholders;

        on<protocol::value>(std::bind(&resolve_action_t::on_value, this, _1, _2, _3));
        on<protocol::error>(std::bind(&resolve_action_t::on_error, this, _1, _2));
    }

    virtual
    void
    discard(const std::error_code& ec) const {
        parent->m_asio.post(std::bind(handle, ec));
    }

private:
    void
    on_value(const std::vector<endpoint_type>& endpoints, int version, const io::graph_root_t&) {
        if(version != client.version()) {
            parent->m_asio.post(std::bind(handle, error::version_mismatch));
            return;
        }

        parent->connect(client, endpoints, handle);
    }

    void
    on_error(int code, const std::string& COCAINE_UNUSED_(reason)) {
        parent->m_asio.post(std::bind(handle, static_cast<error::locator_errors>(code)));
    }
};

class resolve_t::connect_action_t:
    public std::enable_shared_from_this<connect_action_t>
{
    typedef std::vector<endpoint_type>::const_iterator iterator_type;

    resolve_t *const parent;
    basic_client_t&  client;

    // Copied to keep the finalize() iterator valid.
    std::vector<endpoint_type> endpoints;

    // User-supplied completion handler.
    handler_type handle;

    // Used to bootstrap the client.
    std::unique_ptr<tcp::socket> socket;

public:
    connect_action_t(resolve_t *const parent_, basic_client_t& client_,
                     const std::vector<endpoint_type>& endpoints_, handler_type handle_)
    :
        parent(parent_),
        client(client_),
        handle(handle_)
    {
        endpoints.assign(endpoints_.begin(), endpoints_.end());

        // Will be properly disposed of on unsuccessful connection attempt.
        socket = std::make_unique<tcp::socket>(parent->m_asio);
    }

    void
    operator()() {
        async_connect(*socket, endpoints.begin(), endpoints.end(), std::bind(&connect_action_t::finalize,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2
        ));
    }

private:
    void
    finalize(const std::error_code& ec, iterator_type COCAINE_UNUSED_(endpoint)) {
        if(!ec) {
            try {
                client.connect(std::move(socket));
            } catch(const std::system_error& e) {
                // The socket might already be disconnected by this time.
                parent->m_asio.post(std::bind(handle, e.code()));
                return;
            }
        } else {
            socket = nullptr;
        }

        parent->m_asio.post(std::bind(handle, ec));
    }
};

// Resolve

const std::vector<resolve_t::endpoint_type> resolve_t::kDefaultEndpoints = {
    { address::from_string("127.0.0.1"), 10053 }
};

resolve_t::resolve_t(std::unique_ptr<logging::log_t> log, io_service& asio,
                     const std::vector<endpoint_type>& endpoints)
:
    m_log(std::move(log)),
    m_asio(asio)
{
    if(endpoints.empty()) {
        return;
    }

    std::ostringstream stream;
    std::ostream_iterator<char> builder(stream);

    boost::spirit::karma::generate(builder, boost::spirit::karma::stream % ", ", endpoints);

    COCAINE_LOG_DEBUG(m_log, "connecting to remote locator, trying: %s", stream.str());

    connect(m_locator, endpoints, std::bind(&resolve_t::resolve_pending,
        this,
        std::placeholders::_1
    ));
}

void
resolve_t::resolve(basic_client_t& client, const std::string& name, handler_type handle) {
    auto dispatch = std::make_shared<resolve_action_t>(this, client, handle);

    if(!m_locator.session()) {
        m_pending.push_back({dispatch, name});
    } else {
        m_locator.invoke<io::locator::resolve>(dispatch, name);
    }
}

void
resolve_t::connect(basic_client_t& client, const std::vector<endpoint_type>& endpoints,
                   handler_type handle)
{
    m_asio.dispatch(std::bind(&connect_action_t::operator(),
        std::make_shared<connect_action_t>(this, client, endpoints, handle)
    ));
}

void
resolve_t::resolve_pending(const std::error_code& ec) {
    if(ec) {
        COCAINE_LOG_ERROR(m_log, "unable to connect to remote locator - [%d] %s",
            ec.value(), ec.message()
        );

        for(auto it = m_pending.begin(); it != m_pending.end(); ++it) {
            it->dispatch->discard(ec);
        }

        m_pending.clear();
    }

    if(!m_pending.empty()) {
        COCAINE_LOG_DEBUG(m_log, "resolving %d pending service(s)", m_pending.size());

        for(auto it = m_pending.begin(); it != m_pending.end(); ++it) {
            m_locator.invoke<io::locator::resolve>(it->dispatch, it->name);
        }

        m_pending.clear();
    }
}

namespace {

// Resolve errors

struct resolve_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.api.resolve";
    }

    virtual
    auto
    message(int code) const -> std::string {
        switch(code) {
          case cocaine::error::resolve_errors::version_mismatch:
            return "service protocol version is not compatible with the client";
        }

        return "cocaine.api.resolve error";
    }
};

auto
resolve_category() -> const std::error_category& {
    static resolve_category_t instance;
    return instance;
}

} // namespace

namespace cocaine { namespace error {

auto
make_error_code(resolve_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), resolve_category());
}

}} // namespace cocaine::error

namespace cocaine { namespace api {

// Storage

category_traits<storage_t>::ptr_type
storage(context_t& context, const std::string& name) {
    auto it = context.config.storages.find(name);

    if(it == context.config.storages.end()) {
        throw repository_error_t("the '%s' storage is not configured", name);
    }

    return context.get<storage_t>(it->second.type, context, name, it->second.args);
}

}} // namespace cocaine::api
