/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2013-2016 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2016 Other contributors as noted in the AUTHORS file.

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

#include <blackhole/attribute.hpp>
#include <blackhole/attributes.hpp>
#include <blackhole/builder.hpp>
#include <blackhole/config/json.hpp>
#include <blackhole/extensions/facade.hpp>
#include <blackhole/extensions/writer.hpp>
#include <blackhole/formatter/json.hpp>
#include <blackhole/formatter/string.hpp>
#include <blackhole/handler/blocking.hpp>
#include <blackhole/logger.hpp>
#include <blackhole/record.hpp>
#include <blackhole/registry.hpp>
#include <blackhole/root.hpp>
#include <blackhole/sink/console.hpp>
#include <blackhole/sink/file.hpp>
#include <blackhole/sink/socket/tcp.hpp>
#include <blackhole/sink/socket/udp.hpp>
#include <blackhole/wrapper.hpp>

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/traits/attributes.hpp"
#include "cocaine/traits/enum.hpp"
#include "cocaine/traits/vector.hpp"

#include <signal.h>

using namespace cocaine;
using namespace cocaine::logging;
using namespace cocaine::service;

namespace ph = std::placeholders;

namespace {

const std::string DEFAULT_BACKEND("core");

}  // namespace

logging_t::logging_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args) :
    category_type(context, asio, name, args),
    dispatch<io::log_tag>(name),
    verbosity(static_cast<priorities>(args.as_object().at("verbosity", priorities::debug).as_int())),
    signals(std::make_shared<dispatch<io::context_tag>>(name))
{
    const auto backend = args.as_object().at("backend", DEFAULT_BACKEND).as_string();
    // TODO (@esafronov v12.6): Using cache to allow resources reuse.
    // logger = context.log(logging::name_t(backend), format("%s[core]", name), {});
    if (backend == "core") {
        logger = context.log(format("%s[core]", name));
    } else {
        auto reset_logger_fn = [=, &context]() {
            auto registry = blackhole::registry_t::configured();
            registry.add<blackhole::formatter::json_t>();
            registry.add<blackhole::sink::file_t>();
            registry.add<blackhole::sink::socket::tcp_t>();
            registry.add<blackhole::sink::socket::udp_t>();

            std::stringstream stream;
            stream << boost::lexical_cast<std::string>(context.config.logging.loggers);

            auto log = registry.builder<blackhole::config::json_t>(stream)
                .build(backend);

            log.filter([&](const blackhole::record_t& record) -> bool {
                return record.severity() >= context.config.logging.severity;
            });

            logger.reset(new blackhole::root_logger_t(std::move(log)));
        };
        reset_logger_fn();
        signals->on<io::context::os_signal>([=](int signum, siginfo_t){
            if(signum == SIGHUP) {
                reset_logger_fn();
            }
        });
        context.listen(signals, asio);
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
