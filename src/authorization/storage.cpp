#include "storage.hpp"

#include <system_error>

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional/optional.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/numeric.hpp>

#include "cocaine/api/storage.hpp"
#include "cocaine/context.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/format.hpp"
#include "cocaine/format/vector.hpp"
#include "cocaine/idl/storage.hpp"
#include "cocaine/logging.hpp"

namespace cocaine {
namespace authorization {
namespace storage {

namespace {

struct operation_t {
    flags_t flag;

    static
    auto
    from(std::size_t event) -> operation_t {
        switch (event) {
        case io::event_traits<io::storage::read>::id:
        case io::event_traits<io::storage::find>::id:
            return {flags_t::read};
        case io::event_traits<io::storage::write>::id:
        case io::event_traits<io::storage::remove>::id:
            return {flags_t::write};
        }

        __builtin_unreachable();
    }

    auto
    is_read() const -> bool {
        return (flag & flags_t::read) == flags_t::read;
    }

    auto
    is_write() const -> bool {
        return (flag & flags_t::write) == flags_t::write;
    }
};

namespace defaults {

const std::string collection_acls = ".collection-acls";
const std::vector<std::string> collection_acls_tags = {"storage-acls"};

} // namespace defaults
} // namespace

disabled_t::disabled_t(context_t&, const std::string&, const dynamic_t&) {}

auto
disabled_t::verify(std::size_t, const std::string&, const std::string&, const auth::identity_t&, callback_type callback)
    -> void
{
    callback({});
}

enabled_t::enabled_t(context_t& context, const std::string& service, const dynamic_t& args) :
    log(context.log(cocaine::format("authorization/{}/collection", service))),
    backend(api::storage(context, args.as_object().at("backend", "core").as_string())),
    cache_duration(args.as_object().at("cache_duration", 60u).as_uint()),
    cache()
{}

auto
enabled_t::verify(std::size_t event, const std::string& collection, const std::string& key, const auth::identity_t& identity, callback_type callback)
    -> void
{
    if (collection == defaults::collection_acls) {
        // Permissions change.
        // TODO: Do we really want to allow users to change permissions this way? It may lead to
        // format corruption.
        verify(event, key, identity, std::move(callback));
    } else {
        verify(event, collection, identity, std::move(callback));
    }
}

template<typename T, typename P>
auto extract_permissions(const T& credentials, P& perms) -> std::size_t {
    using value_type = typename T::value_type;

    if (credentials.empty()) {
        return flags_t::none;
    }

    return boost::accumulate(
        credentials,
        static_cast<std::size_t>(flags_t::both),
        [&](std::size_t acc, value_type id) {
            return acc & perms[id];
        }
    );
}

auto
enabled_t::verify(std::size_t event, const std::string& collection, const auth::identity_t& identity, callback_type callback)
    -> void
{
    using auth::cid_t;
    using auth::uid_t;

    auto& cids = identity.cids();
    auto& uids = identity.uids();

    auto on_metainfo = [=](metainfo_t metainfo) mutable {
        COCAINE_LOG_DEBUG(log, "read metainfo with {} cid and {} uid records",
            metainfo.c_perms.size(), metainfo.u_perms.size());

        const auto op = operation_t::from(event);
        // Owned collection must have at least one record. Otherwise it is treated as common, until
        // someone performs write operation over.
        if (metainfo.empty()) {
            if (op.is_write() && (cids.size() > 0 || uids.size() > 0)) {
                COCAINE_LOG_INFO(log, "initializing ACL for '{}' collection for cid(s) {} and uid(s) {}",
                    collection, cids, uids);

                for (auto cid : cids) {
                    metainfo.c_perms[cid] = flags_t::both;
                }
                for (auto uid : uids) {
                    metainfo.u_perms[uid] = flags_t::both;
                }
                backend->put<metainfo_t>(defaults::collection_acls, collection, metainfo, defaults::collection_acls_tags, [=](std::future<void> future) mutable {
                    try {
                        future.get();
                        callback(std::error_code());
                    } catch (const std::system_error& err) {
                        callback(err.code());
                    }
                });
                return;
            }
        } else {
            auto c_perm = extract_permissions(cids, metainfo.c_perms);
            auto u_perm = extract_permissions(uids, metainfo.u_perms);
            auto allowed = ((c_perm | u_perm) & op.flag) == op.flag;

            if (!allowed) {
                callback(make_error_code(std::errc::permission_denied));
                return;
            }
        }

        callback(std::error_code());
    };

    auto metainfo = cache.apply([&](const std::map<std::string, cached<metainfo_t>>& cache)
        -> boost::optional<metainfo_t>
    {
        auto it = cache.find(collection);
        if (it == std::end(cache)) {
            return boost::none;
        }

        cached<metainfo_t>::tag_t tag;
        metainfo_t metainfo;
        std::tie(tag, metainfo) = it->second.get();
        if (tag == cached<metainfo_t>::tag_t::expired) {
            return boost::none;
        } else {
            return metainfo;
        }
    });

    if (metainfo) {
        COCAINE_LOG_DEBUG(log, "reading ACL metainfo for collection '{}' from cache", collection);
        on_metainfo(std::move(*metainfo));
    } else {
        COCAINE_LOG_DEBUG(log, "reading ACL metainfo for collection '{}'", collection);
        backend->get<metainfo_t>(defaults::collection_acls, collection, [=](std::future<metainfo_t> future) mutable {
            metainfo_t metainfo;
            try {
                metainfo = future.get();
            } catch (const std::system_error& err) {
                if (err.code() != std::errc::no_such_file_or_directory) {
                    COCAINE_LOG_ERROR(log, "failed to read ACL metainfo for collection '{}': {}",
                        collection, error::to_string(err));
                    callback(err.code());
                    return;
                }
            } catch (const msgpack::unpack_error&) {
                COCAINE_LOG_ERROR(log, "failed to read ACL metainfo for collection '{}': invalid ACL framing",
                    collection);
                callback(make_error_code(error::invalid_acl_framing));
                return;
            } catch (const std::bad_cast&) {
                COCAINE_LOG_ERROR(log, "failed to read ACL metainfo for collection '{}': invalid ACL framing",
                    collection);
                callback(make_error_code(error::invalid_acl_framing));
                return;
            }

            cache.apply([&](std::map<std::string, cached<metainfo_t>>& cache) {
                auto acl = cached<metainfo_t>(metainfo, cache_duration);
                auto it = cache.find(collection);
                if (it == std::end(cache)) {
                    cache.insert({collection, acl});
                } else {
                    it->second = acl;
                }
            });

            on_metainfo(std::move(metainfo));
        });
    }
}

} // namespace storage
} // namespace authorization
} // namespace cocaine
