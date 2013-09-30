/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_ADHOC_GATEWAY_HPP
#define COCAINE_ADHOC_GATEWAY_HPP

#include "cocaine/api/gateway.hpp"

#include <random>

namespace cocaine { namespace gateway {

class adhoc_t:
    public api::gateway_t
{
    public:
        adhoc_t(context_t& context, const std::string& name, const Json::Value& args);

        virtual
       ~adhoc_t();

        virtual
        api::resolve_result_type
        resolve(const std::string& name) const;

        virtual
        void
        consume(const std::string& uuid, const std::string& name, const api::resolve_result_type& info);

        virtual
        void
        cleanup(const std::string& uuid, const std::string& name);

    private:
        const std::unique_ptr<logging::log_t> m_log;

#if defined(__clang__) || defined(HAVE_GCC46)
        mutable std::default_random_engine m_random_generator;
#else
        mutable std::minstd_rand0 m_random_generator;
#endif

        struct remote_service_t {
            std::string uuid;
            api::resolve_result_type info;
        };

        typedef std::multimap<
            std::string,
            remote_service_t
        > remote_service_map_t;

        remote_service_map_t m_remote_services;
};

}} // namespace cocaine::gateway

#endif
