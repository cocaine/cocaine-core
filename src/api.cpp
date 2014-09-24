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

#include <boost/asio/connect.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;

// Connect

namespace cocaine { namespace api { namespace details {

basic_client_t::basic_client_t(const basic_client_t& other) {
    *this = other;
}

basic_client_t::~basic_client_t() {
    if(is_connected()) {
        m_session->detach();
    }
}

basic_client_t&
basic_client_t::operator=(const basic_client_t& rhs) {
    if((m_session = rhs.m_session) == nullptr) {
        return *this;
    }

    m_session->signals.shutdown.connect(std::bind(&basic_client_t::on_interrupt,
        this,
        std::placeholders::_1
    ));

    return *this;
}

bool
basic_client_t::is_connected() const {
    return static_cast<bool>(m_session);
}

const session_t&
basic_client_t::session() const {
    if(!is_connected()) {
        throw cocaine::error_t("client is not connected");
    }

    return *m_session;
}

void
basic_client_t::on_interrupt(const boost::system::error_code& COCAINE_UNUSED_(ec)) {
    m_session->detach();
    m_session = nullptr;
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

    m_session->signals.shutdown.connect(std::bind(&basic_client_t::on_interrupt,
        this,
        std::placeholders::_1
    ));

    m_session->pull();
}

}}} // namespace cocaine::api::details

using namespace cocaine::api;

// Resolve internals

class resolve_t::resolve_action_t:
    public dispatch<io::event_traits<io::locator::resolve>::upstream_type>
{
    resolve_t* parent;
    details::basic_client_t& client;
    handler_type handle;

public:
    resolve_action_t(resolve_t* parent_, details::basic_client_t& client_, handler_type handle_):
        dispatch<io::event_traits<io::locator::resolve>::upstream_type>("resolve"),
        parent(parent_),
        client(client_),
        handle(handle_)
    {
        typedef io::protocol<io::event_traits<io::locator::resolve>::upstream_type>::scope protocol;

        using namespace std::placeholders;

        on<protocol::chunk>(std::bind(&resolve_action_t::on_chunk, this, _1, _2, _3));
        on<protocol::error>(std::bind(&resolve_action_t::on_error, this, _1, _2));
        on<protocol::choke>(std::bind(&resolve_action_t::on_choke, this));
    }

    virtual
    void
    discard(const boost::system::error_code& ec) const {
        parent->m_asio.post(std::bind(handle, ec));
    }

private:
    void
    on_chunk(const std::vector<endpoint_type>& endpoints, int version, const io::graph_basis_t&) {
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

    void
    on_choke() {
        // Do nothing.
    }
};

class resolve_t::connect_action_t:
    public std::enable_shared_from_this<connect_action_t>
{
    typedef std::vector<endpoint_type>::const_iterator iterator_type;

    resolve_t* parent;
    details::basic_client_t& client;
    handler_type handle;

    // Copied to keep the finalize() iterator valid.
    std::vector<endpoint_type> endpoints;

    // Used to bootstrap the client.
    std::unique_ptr<tcp::socket> socket;

public:
    connect_action_t(resolve_t* parent_, details::basic_client_t& client_,
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
    finalize(const boost::system::error_code& ec, iterator_type COCAINE_UNUSED_(endpoint)) {
        if(ec == boost::system::errc::success) {
            try {
                client.connect(std::move(socket));
            } catch(const boost::system::system_error& e) {
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
    std::ostream_iterator<endpoint_type> builder(stream, ", ");

    std::copy(endpoints.begin(), endpoints.end(), builder);

    COCAINE_LOG_DEBUG(m_log, "connecting to remote locator, trying: %s", stream.str());

    connect(m_locator, endpoints, std::bind(&resolve_t::resolve_pending,
        this,
        std::placeholders::_1
    ));
}

void
resolve_t::resolve(details::basic_client_t& client, const std::string& name, handler_type handle) {
    auto dispatch = std::make_shared<resolve_action_t>(this, client, handle);

    if(!m_locator.is_connected()) {
        m_pending.push_back({dispatch, name});
    } else {
        m_locator.invoke<io::locator::resolve>(dispatch, name);
    }
}

void
resolve_t::connect(details::basic_client_t& client, const std::vector<endpoint_type>& endpoints,
                   handler_type handle)
{
    m_asio.dispatch(std::bind(&connect_action_t::operator(),
        std::make_shared<connect_action_t>(this, client, endpoints, handle)
    ));
}

void
resolve_t::resolve_pending(const boost::system::error_code& ec) {
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
    public boost::system::error_category
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
resolve_category() -> const boost::system::error_category& {
    static resolve_category_t instance;
    return instance;
}

} // namespace

namespace cocaine { namespace error {

auto
make_error_code(resolve_errors code) -> boost::system::error_code {
    return boost::system::error_code(static_cast<int>(code), resolve_category());
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
