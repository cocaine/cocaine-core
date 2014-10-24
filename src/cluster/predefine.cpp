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
#include "cocaine/logging.hpp"

#include "cocaine/detail/unique_id.hpp"

#include <boost/asio/io_service.hpp>

#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_stream.hpp>
#include <boost/spirit/include/karma_string.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine::cluster;

namespace cocaine {

namespace ph = std::placeholders;

template<>
struct dynamic_converter<predefine_cfg_t> {
    typedef predefine_cfg_t result_type;

    static
    result_type
    convert(const dynamic_t& source) {
        result_type result;

        const dynamic_t& nodes = source.as_object().at("nodes", dynamic_t::object_t());

        if(nodes.as_object().empty()) {
            throw cocaine::error_t("at least one node should be specified for predefined cluster");
        }

        io_service service;

        tcp::resolver resolver(service);
        tcp::resolver::iterator it, end;

        for(auto node = nodes.as_object().begin(); node != nodes.as_object().end(); ++node) {
            try {
                it = resolver.resolve(tcp::resolver::query(
                    node->first, std::to_string(node->second.as_uint())
                ));
            } catch(const boost::system::system_error& e) {
#if defined(HAVE_GCC48)
                std::throw_with_nested(cocaine::error_t("unable to determine predefined host endpoints"));
#else
                throw cocaine::error_t("unable to determine predefined host endpoints");
#endif
            }

            // Generate a random UUID for each predefined node.
            result.endpoints[unique_id_t().string()] = std::vector<tcp::endpoint>(it, end);
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
    for(auto it = m_cfg.endpoints.begin(); it != m_cfg.endpoints.end(); ++it) {
        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::stream % ", ", it->second);

        COCAINE_LOG_INFO(m_log, "resolved node endpoints: %s", stream.str())(
            "uuid", it->first
        );
    }

    m_timer.expires_from_now(m_cfg.interval);
    m_timer.async_wait(std::bind(&predefine_t::on_announce, this, ph::_1));
}

predefine_t::~predefine_t() {
    m_timer.cancel();
}

void
predefine_t::on_announce(const boost::system::error_code& ec) {
    if(ec == boost::asio::error::operation_aborted) {
        return;
    }

    for(auto it = m_cfg.endpoints.begin(); it != m_cfg.endpoints.end(); ++it) {
        m_locator.link_node(it->first, it->second);
    }

    m_timer.expires_from_now(m_cfg.interval);
    m_timer.async_wait(std::bind(&predefine_t::on_announce, this, ph::_1));
}
