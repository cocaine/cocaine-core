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

#include "cocaine/detail/service/storage.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/scope/holder.hpp>
#include <blackhole/wrapper.hpp>

#include "cocaine/api/storage.hpp"
#include "cocaine/api/controller.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/dynamic/dynamic.hpp"
#include "cocaine/middleware/auth.hpp"
#include "cocaine/middleware/headers.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

namespace ph = std::placeholders;

namespace {

template<class T>
void
abort_deferred(cocaine::deferred<T>& def, const std::error_code& ec, const std::string& reason) {
    try {
        def.abort(ec, reason);
    } catch (const std::system_error&) {
        // pass, the only reason of exception is detached session
    }
}

struct audit_middleware_t {
    std::shared_ptr<logging::logger_t> logger;

    template<typename F, typename Event, typename... Args>
    auto
    operator()(F fn, Event, Args&&... args) ->
        decltype(fn(std::forward<Args>(args)..., logger))
    {
        const auto log = make_logger(Event(), args...);

        try {
            return fn(std::forward<Args>(args)..., log);
        } catch (const std::system_error& err) {
            COCAINE_LOG_WARNING(log, "failed to complete '{}' operation", Event::alias(), blackhole::attribute_list{
                {"code", err.code().value()},
                {"error", error::to_string(err)},
            });
            throw;
        }
    }

private:
    template<typename Event, typename... Args>
    auto
    make_logger(Event, const std::string& collection, const std::string& key, Args&&... args)
        -> std::shared_ptr<logging::logger_t>
    {
        const auto uids = extract_uids(args...);
        return std::make_shared<blackhole::wrapper_t>(*logger, blackhole::attributes_t{
            {"event", std::string(Event::alias())},
            {"collection", collection},
            {"key", key},
            {"uids", cocaine::format("[{}]", boost::join(uids | boost::adaptors::transformed(static_cast<std::string(*)(api::uid_t)>(std::to_string)), ";"))},
        });
    }

    template<typename... Args>
    auto
    make_logger(io::storage::find, const std::string& collection, Args&&... args)
        -> std::shared_ptr<logging::logger_t>
    {
        const auto uids = extract_uids(args...);
        return std::make_shared<blackhole::wrapper_t>(*logger, blackhole::attributes_t{
            {"event", std::string(io::storage::find::alias())},
            {"collection", collection},
            {"uids", cocaine::format("[{}]", boost::join(uids | boost::adaptors::transformed(static_cast<std::string(*)(api::uid_t)>(std::to_string)), ";"))},
        });
    }

    template<typename... Args>
    auto
    extract_uids(const Args&... args) -> const std::vector<api::uid_t>& {
        return std::get<sizeof...(Args) - 1>(std::tie(args...));
    }
};

} // namespace

storage_t::storage_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args):
    category_type(context, asio, name, args),
    dispatch<storage_tag>(name)
{
    auto backend = api::storage(context, args.as_object().at("backend", "core").as_string());

    auto audit = std::shared_ptr<logging::logger_t>(context.log("audit", {{"service", name}}));
    auto middleware = middleware::auth_t(context, name);
    auto controller = api::controller::collection(context, name);

    on<storage::read>()
        .with_middleware(middleware)
        .with_middleware(middleware::drop_headers_t())
        .with_middleware(audit_middleware_t{audit})
        .execute([=](
            const std::string& collection,
            const std::string& key,
            const std::vector<api::uid_t>& uids,
            const std::shared_ptr<logging::logger_t>& log)
        {
            controller->verify<io::storage::read>(collection, key, uids);

            cocaine::deferred<std::string> deferred;

            backend->read(collection, key, [=](std::future<std::string> future) mutable {
                try {
                    auto result = future.get();
                    COCAINE_LOG_INFO(log, "completed 'read' operation", {
                        {"size", result.size()},
                    });

                    deferred.write(std::move(result));
                } catch (const std::system_error& err) {
                    COCAINE_LOG_WARNING(log, "failed to complete 'read' operation", {
                        {"code", err.code().value()},
                        {"error", error::to_string(err)},
                    });
                    abort_deferred(deferred, err.code(), err.what());
                }
            });

            return deferred;
        });

    on<storage::write>()
        .with_middleware(middleware)
        .with_middleware(middleware::drop_headers_t())
        .with_middleware(audit_middleware_t{audit})
        .execute([=](
            const std::string& collection,
            const std::string& key,
            const std::string& blob,
            const std::vector<std::string>& tags,
            const std::vector<api::uid_t>& uids,
            const std::shared_ptr<logging::logger_t>& log)
    {
        controller->verify<io::storage::write>(collection, key, uids);

        cocaine::deferred<void> deferred;

        const auto size = blob.size();
        backend->write(collection, key, blob, tags, [=](std::future<void> future) mutable {
            try {
                future.get();
                COCAINE_LOG_INFO(log, "completed 'write' operation", {
                    {"size", size},
                });

                deferred.close();
            } catch (const std::system_error& err) {
                COCAINE_LOG_WARNING(log, "failed to complete 'write' operation", {
                    {"code", err.code().value()},
                    {"error", error::to_string(err)},
                });
                abort_deferred(deferred, err.code(), err.what());
            }
        });

        return deferred;
    });

    on<storage::remove>()
        .with_middleware(middleware)
        .with_middleware(middleware::drop_headers_t())
        .with_middleware(audit_middleware_t{audit})
        .execute([=](
            const std::string& collection,
            const std::string& key,
            const std::vector<api::uid_t>& uids,
            const std::shared_ptr<logging::logger_t>& log)
    {
        controller->verify<io::storage::remove>(collection, key, uids);

        cocaine::deferred<void> deferred;

        backend->remove(collection, key, [=](std::future<void> future) mutable {
            try {
                future.get();
                COCAINE_LOG_INFO(log, "completed 'remove' operation");

                deferred.close();
            } catch (const std::system_error& err) {
                COCAINE_LOG_WARNING(log, "failed to complete 'remove' operation", {
                    {"code", err.code().value()},
                    {"error", error::to_string(err)},
                });
                abort_deferred(deferred, err.code(), err.what());
            }
        });

        return deferred;
    });

    on<storage::find>()
        .with_middleware(middleware)
        .with_middleware(middleware::drop_headers_t())
        .with_middleware(audit_middleware_t{audit})
        .execute([=](
            const std::string& collection,
            const std::vector<std::string>& tags,
            const std::vector<api::uid_t>& uids,
            const std::shared_ptr<logging::logger_t>& log)
    {
        controller->verify<io::storage::find>(collection, "", uids);

        cocaine::deferred<std::vector<std::string>> deferred;

        backend->find(collection, tags, [=](std::future<std::vector<std::string>> future) mutable {
            try {
                auto result = future.get();
                COCAINE_LOG_INFO(log, "completed 'find' operation", {
                    {"keys", result.size()},
                });

                deferred.write(std::move(result));
            } catch (const std::system_error& err) {
                COCAINE_LOG_WARNING(log, "failed to complete 'find' operation", {
                    {"code", err.code().value()},
                    {"error", error::to_string(err)},
                });
                abort_deferred(deferred, err.code(), err.what());
            }
        });

        return deferred;
    });
}

auto
storage_t::prototype() const -> const basic_dispatch_t& {
    return *this;
}
