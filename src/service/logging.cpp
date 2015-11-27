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

#include "cocaine/detail/service/logging.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/attributes.hpp"
#include "cocaine/traits/enum.hpp"
#include "cocaine/traits/vector.hpp"

#include <blackhole/logger.hpp>
#include <blackhole/attributes.hpp>

using namespace cocaine;
using namespace cocaine::logging;
using namespace cocaine::service;

namespace ph = std::placeholders;

logging_t::logging_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args):
    category_type(context, asio, name, args),
    dispatch<io::log_tag>(name),
    verbosity(static_cast<priorities>(args.as_object().at("verbosity", priorities::debug).as_int()))
{
    const auto backend = args.as_object().at("backend", "core").as_string();

    if (backend == "core") {
        logger = context.log(format("%s[core]", name));
    } else {
        throw std::runtime_error("not implemented");
    }

    on<io::log::emit>(std::bind(&logging_t::on_emit, this, ph::_1, ph::_2, ph::_3, ph::_4));
    on<io::log::verbosity>(std::bind(&logging_t::on_verbosity, this));
}

auto
logging_t::prototype() const -> const io::basic_dispatch_t& {
    return *this;
}

void
logging_t::on_emit(logging::priorities level, std::string source, std::string message,
    blackhole::attributes_t attributes)
{
    if (level < on_verbosity()) {
        return;
    }

    blackhole::attribute_list list{{"source", source}};
    for (auto attribute : attributes) {
        list.emplace_back(attribute);
    }
    
    blackhole::attribute_pack pack{list};

    logger->log(static_cast<int>(level), message, pack);
}

logging::priorities
logging_t::on_verbosity() const {
    return verbosity;
}
