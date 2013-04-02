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

#include "cocaine/essentials/services/logging.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace std::placeholders;

namespace cocaine { namespace io {
    using cocaine::logging::priorities;

    template<>
    struct type_traits<priorities> {
        template<class Stream>
        static inline
        void
        pack(msgpack::packer<Stream>& packer, const priorities& source) {
            packer << static_cast<int>(source);
        }

        static inline
        void
        unpack(const msgpack::object& unpacked, priorities& target) {
            target = static_cast<priorities>(unpacked.as<int>());
        }
    };
}}

logging_t::logging_t(context_t& context,
                     reactor_t& reactor,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, reactor, name, args)
{
    auto logger = std::ref(context.logger());

    using cocaine::logging::logger_concept_t;

    on<io::logging::emit     >("emit",      std::bind(&logger_concept_t::emit,      logger, _1, _2, _3));
    on<io::logging::verbosity>("verbosity", std::bind(&logger_concept_t::verbosity, logger));
}

