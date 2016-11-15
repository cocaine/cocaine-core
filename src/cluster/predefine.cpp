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

#include "cocaine/detail/cluster/predefine.hpp"


#include "cocaine/context.hpp"
#include "cocaine/context/signal.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/vector.hpp"

#include <asio/io_service.hpp>

#include <blackhole/logger.hpp>

using namespace cocaine::io;
using namespace cocaine::cluster;

using namespace asio;
using namespace asio::ip;

using blackhole::attribute_list;

namespace cocaine {

namespace ph = std::placeholders;

template<>
struct dynamic_converter<predefine_cfg_t> {
    typedef predefine_cfg_t result_type;

    static
    result_type
    convert(const dynamic_t& source) {
        result_type result;

        const dynamic_t& nodes = source.as_object().at("nodes", dynamic_t::empty_object);

        if(nodes.as_object().empty()) {
            throw cocaine::error_t("no nodes have been specified");
        }

        io_service service;

        tcp::resolver resolver(service);
        tcp::resolver::iterator it, end;

        for(auto node = nodes.as_object().begin(); node != nodes.as_object().end(); ++node) {
            auto addr = node->second.as_string();

            try {
                it = resolver.resolve(tcp::resolver::query(
                    // TODO: A better way to parse this.
                    addr.substr(0, addr.rfind(":")), addr.substr(addr.rfind(":") + 1)
                ));
            } catch(const std::system_error& e) {
                throw std::system_error(e.code(), "unable to determine predefined node endpoints");
            }

            result.endpoints[node->first] = std::vector<tcp::endpoint>(it, end);
        }

        result.interval = boost::posix_time::seconds(
            source.as_object().at("interval", 5u).as_uint()
        );

        return result;
    }
};

} // namespace cocaine

predefine_t::predefine_t(context_t& context, interface& locator, const std::string& name, const dynamic_t& args):
    category_type(context, locator, name, args),
    m_log(context.log(name)),
    m_locator(locator),
    m_cfg(args.to<predefine_cfg_t>()),
    m_timer(locator.asio())
{
    m_signals = std::make_shared<dispatch<context_tag>>(name);
    m_signals->on<io::context::prepared>(std::bind(&predefine_t::on_announce, this, std::error_code()));

    context.signal_hub().listen(m_signals, m_locator.asio());
}

predefine_t::~predefine_t() {
    m_timer.cancel();
}

void
predefine_t::on_announce(const std::error_code& ec) {
    if(ec == asio::error::operation_aborted) {
        return;
    }

    for(auto it = m_cfg.endpoints.begin(); it != m_cfg.endpoints.end(); ++it) {
        m_locator.link_node(it->first, it->second);
    }

    m_timer.expires_from_now(m_cfg.interval);
    m_timer.async_wait(std::bind(&predefine_t::on_announce, this, ph::_1));
}
