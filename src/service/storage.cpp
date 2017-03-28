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

#include "cocaine/api/authorization/storage.hpp"
#include "cocaine/api/storage.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/dynamic/dynamic.hpp"
#include "cocaine/format/vector.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/middleware/auth.hpp"
#include "cocaine/middleware/headers.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

namespace ph = std::placeholders;

namespace {

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
        auto identity = extract_identity(args...);
        auto cids = identity.cids();
        auto uids = identity.uids();
        return std::make_shared<blackhole::wrapper_t>(*logger, blackhole::attributes_t{
            {"event", std::string(Event::alias())},
            {"collection", collection},
            {"key", key},
            {"cids", cocaine::format("{}", cids)},
            {"uids", cocaine::format("{}", uids)},
        });
    }

    template<typename... Args>
    auto
    make_logger(io::storage::find, const std::string& collection, Args&&... args)
        -> std::shared_ptr<logging::logger_t>
    {
        auto identity = extract_identity(args...);
        auto cids = identity.cids();
        auto uids = identity.uids();
        return std::make_shared<blackhole::wrapper_t>(*logger, blackhole::attributes_t{
            {"event", std::string(io::storage::find::alias())},
            {"collection", collection},
            {"cids", cocaine::format("{}", cids)},
            {"uids", cocaine::format("{}", uids)},
        });
    }

    template<typename... Args>
    auto
    extract_identity(const Args&... args) -> const auth::identity_t& {
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
    auto authorization = api::authorization::storage(context, name);

    on<storage::read>()
        .with_middleware(middleware)
        .with_middleware(middleware::drop_headers_t())
        .with_middleware(audit_middleware_t{audit})
        .execute([=](
            const std::string& collection,
            const std::string& key,
            const auth::identity_t& identity,
            const std::shared_ptr<logging::logger_t>& log)
        {
            cocaine::deferred<std::string> deferred;
            authorization->verify<io::storage::read>(collection, key, identity, [=](std::error_code ec) mutable {
                if (ec) {
                    COCAINE_LOG_WARNING(log, "failed to complete 'read' operation", {
                        {"code", ec.value()},
                        {"error", ec.message()},
                    });
                    deferred.abort(ec, "Permission denied");
                    return;
                }

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
                        deferred.abort(err.code(), err.what());
                    }
                });
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
            const auth::identity_t& identity,
            const std::shared_ptr<logging::logger_t>& log)
    {
        cocaine::deferred<void> deferred;

        authorization->verify<io::storage::write>(collection, key, identity, [=](std::error_code ec) mutable {
            if (ec) {
                COCAINE_LOG_WARNING(log, "failed to complete 'write' operation", {
                    {"code", ec.value()},
                    {"error", ec.message()},
                });
                deferred.abort(ec, "Permission denied");
                return;
            }

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
                    deferred.abort(err.code(), err.what());
                }
            });
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
            const auth::identity_t& identity,
            const std::shared_ptr<logging::logger_t>& log)
    {
        cocaine::deferred<void> deferred;

        authorization->verify<io::storage::remove>(collection, key, identity, [=](std::error_code ec) mutable {
            if (ec) {
                COCAINE_LOG_WARNING(log, "failed to complete 'remove' operation", {
                    {"code", ec.value()},
                    {"error", ec.message()},
                });
                deferred.abort(ec, "Permission denied");
                return;
            }

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
                    deferred.abort(err.code(), err.what());
                }
            });
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
            const auth::identity_t& identity,
            const std::shared_ptr<logging::logger_t>& log)
    {
        cocaine::deferred<std::vector<std::string>> deferred;

        authorization->verify<io::storage::find>(collection, "", identity, [=](std::error_code ec) mutable {
            if (ec) {
                COCAINE_LOG_WARNING(log, "failed to complete 'find' operation", {
                    {"code", ec.value()},
                    {"error", ec.message()},
                });
                deferred.abort(ec, "Permission denied");
                return;
            }

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
                    deferred.abort(err.code(), err.what());
                }
            });
        });

        return deferred;
    });
}

auto
storage_t::prototype() -> basic_dispatch_t& {
    return *this;
}
