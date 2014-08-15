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

#include "cocaine/detail/cluster/multicast.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/detail/actor.hpp"

#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/tuple.hpp"
#include "cocaine/traits/vector.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/multicast.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine;
using namespace cocaine::api;
using namespace cocaine::cluster;

namespace cocaine {

template<>
struct dynamic_converter<address> {
    typedef address result_type;

    static
    result_type
    convert(const dynamic_t& source) {
        return address::from_string(source.as_string());
    }
};

template<>
struct dynamic_converter<multicast_config_t> {
    typedef multicast_config_t result_type;

    static
    result_type
    convert(const dynamic_t& source) {
        result_type result;

        try {
            result.endpoint = udp::endpoint(
                source.as_object().at("group").to<address>(),
                source.as_object().at("port", 10053u).as_uint()
            );
        } catch(std::out_of_range& e) {
            throw cocaine::error_t("no multicast group has been specified");
        }

        result.interval = boost::posix_time::seconds(
            source.as_object().at("interval", 5u).as_uint()
        );

        return result;
    }
};

} // namespace cocaine

struct multicast_t::packet_t {
    typedef std::tuple<
     /* Node ID. */
        std::string,
     /* A list of node endpoints, sorted according to RFC3484. */
        std::vector<tcp::endpoint>
    > tuple_type;

    std::array<char, 1024> buffer;
    udp::endpoint origin;
};

multicast_t::multicast_t(context_t& context, interface& locator, const std::string& name, const dynamic_t& args):
    category_type(context, locator, name, args),
    m_context(context),
    m_locator(locator),
    m_log(context.log(name)),
    m_config(args.to<multicast_config_t>()),
    m_socket(locator.asio()),
    m_timer(locator.asio())
{
    m_socket.open(m_config.endpoint.protocol());
    m_socket.set_option(socket_base::reuse_address(true));

    if(m_config.endpoint.address().is_v4()) {
        m_socket.bind(udp::endpoint(address_v4::any(), m_config.endpoint.port()));
    } else {
        m_socket.bind(udp::endpoint(address_v6::any(), m_config.endpoint.port()));
    }

    if(args.as_object().count("interface")) {
        auto interface = args.as_object().at("interface");

        if(m_config.endpoint.address().is_v4()) {
            m_socket.set_option(multicast::outbound_interface(interface.to<address>().to_v4()));
        } else {
            m_socket.set_option(multicast::outbound_interface(interface.as_uint()));
        }
    }

    m_socket.set_option(multicast::enable_loopback(true));
    m_socket.set_option(multicast::hops(args.as_object().at("hops", 1u).as_uint()));

    COCAINE_LOG_INFO(m_log, "joining multicast group '%s'", m_config.endpoint)(
        "uuid", m_locator.uuid()
    );

    m_socket.set_option(multicast::join_group(m_config.endpoint.address()));

    auto packet = std::make_shared<packet_t>();

    m_socket.async_receive_from(buffer(packet->buffer.data(), packet->buffer.size()), packet->origin,
        std::bind(&multicast_t::receive, this, std::placeholders::_1, std::placeholders::_2, packet)
    );

    m_timer.expires_from_now(m_config.interval);
    m_timer.async_wait(std::bind(&multicast_t::publish, this, std::placeholders::_1));
}

multicast_t::~multicast_t() {
    for(auto it = m_expirations.begin(); it != m_expirations.end(); ++it) {
        it->second->cancel();
    }

    m_expirations.clear();

    m_timer.cancel();
    m_socket.close();
}

void
multicast_t::publish(const boost::system::error_code& ec) {
    if(ec == error::operation_aborted) {
        return;
    }

    auto actor = m_context.locate("locator");

    if(!actor) {
        COCAINE_LOG_ERROR(m_log, "unable to announce local endpoints: locator is not available");
        return;
    }

    auto endpoints = actor.get().endpoints();

    if(!endpoints.empty()) {
        COCAINE_LOG_DEBUG(m_log, "announcing %d local endpoint(s)", endpoints.size())(
            "uuid", m_locator.uuid()
        );

        msgpack::sbuffer target;
        msgpack::packer<msgpack::sbuffer> packer(target);

        io::type_traits<packet_t::tuple_type>::pack(packer, std::make_tuple(
            m_locator.uuid(),
            endpoints
        ));

        try {
            m_socket.send_to(buffer(target.data(), target.size()), m_config.endpoint);
        } catch(const boost::system::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to announce local endpoints: [%d] %s", e.code().value(), e.code().message());
        }
    } else {
        COCAINE_LOG_ERROR(m_log, "unable to announce local endpoints: no endpoints have been found");
    }

    m_timer.expires_from_now(m_config.interval);
    m_timer.async_wait(std::bind(&multicast_t::publish, this, std::placeholders::_1));
}

void
multicast_t::receive(const boost::system::error_code& ec, size_t rcvd, const std::shared_ptr<packet_t>& ptr) {
    if(ec == error::operation_aborted) {
        return;
    } else if(ec) {
        COCAINE_LOG_ERROR(m_log, "unexpected error in multicast_t::receive(): [%d] %s", ec.value(), ec.message());
        return;
    }

    msgpack::unpacked unpacked;

    try {
        msgpack::unpack(&unpacked, ptr->buffer.data(), rcvd);
    } catch(const msgpack::unpack_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to unpack an announce");
        return;
    }

    std::string uuid;
    std::vector<tcp::endpoint> endpoints;

    try {
        io::type_traits<packet_t::tuple_type>::unpack(unpacked.get(), std::tie(uuid, endpoints));
    } catch(const msgpack::type_error& e) {
        COCAINE_LOG_ERROR(m_log, "unable to unpack an announce");
        return;
    }

    if(uuid != m_locator.uuid()) {
        COCAINE_LOG_DEBUG(m_log, "received %d remote endpoint(s) from %s", endpoints.size(), ptr->origin)(
            "uuid", uuid
        );

        auto& expiration = m_expirations[uuid];

        if(!expiration) {
            expiration = std::make_unique<deadline_timer>(m_locator.asio());

            // Link a new node only when seen for the first time.
            m_locator.link_node(uuid, endpoints);
        }

        expiration->expires_from_now(m_config.interval * 3);
        expiration->async_wait(std::bind(&multicast_t::cleanup, this, std::placeholders::_1, uuid));
    }

    auto packet = std::make_shared<packet_t>();

    m_socket.async_receive_from(buffer(packet->buffer.data(), packet->buffer.size()), packet->origin,
        std::bind(&multicast_t::receive, this, std::placeholders::_1, std::placeholders::_2, packet)
    );
}

void
multicast_t::cleanup(const boost::system::error_code& ec, const std::string& uuid) {
    if(ec == error::operation_aborted) {
        return;
    }

    COCAINE_LOG_ERROR(m_log, "remote endpoints have expired")(
        "uuid", uuid
    );

    m_locator.drop_node(uuid);
    m_expirations.erase(uuid);
}
