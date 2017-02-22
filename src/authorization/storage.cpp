#include "storage.hpp"

#include <system_error>

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include "cocaine/api/storage.hpp"
#include "cocaine/context.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/format.hpp"
#include "cocaine/idl/storage.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/traits/enum.hpp"
#include "cocaine/traits/map.hpp"

namespace cocaine {
namespace authorization {
namespace storage {

namespace {

enum flags_t: std::size_t {
    read  = 0x01,
    write = 0x02,
    both  = read | write
};

using metainfo_t = std::map<uid_t, flags_t>;

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

} // namespace defaults
} // namespace

disabled_t::disabled_t(context_t&, const std::string&, const dynamic_t&) {}

auto
disabled_t::verify(std::size_t, const std::string&, const std::string&, const auth::identity_t&) -> void {}

auto
disabled_t::verify(std::size_t, const std::string&, const std::string&, const auth::identity_t&, callback_type callback)
    -> void
{
    callback({});
}

enabled_t::enabled_t(context_t& context, const std::string& service, const dynamic_t& args) :
    log(context.log(cocaine::format("authorization/{}/collection", service))),
    backend(api::storage(context, args.as_object().at("backend", "core").as_string()))
{}

auto
enabled_t::verify(std::size_t event, const std::string& collection, const std::string& key, const auth::identity_t& identity) -> void {
    auto& uids = identity.uids();
    if (collection == defaults::collection_acls) {
        // Permissions change.
        // TODO: Do we really want to allow users to change permissions this way? It may lead to
        // format corruption.
        return verify(event, key, uids);
    } else {
        return verify(event, collection, uids);
    }
}

auto
enabled_t::verify(std::size_t event, const std::string& collection, const std::vector<auth::uid_t>& uids) -> void {
    if (uids.empty()) {
        throw std::system_error(make_error_code(error::unauthorized));
    }

    metainfo_t metainfo;
    COCAINE_LOG_DEBUG(log, "reading ACL metainfo for collection '{}'", collection);
    try {
        metainfo = backend->get<metainfo_t>(defaults::collection_acls, collection).get();
    } catch (const std::system_error& err) {
        if (err.code() != std::errc::no_such_file_or_directory) {
            COCAINE_LOG_ERROR(log, "failed to read ACL metainfo for collection '{}': {}",
                collection, error::to_string(err));
            throw;
        }
    }

    const auto op = operation_t::from(event);
    // Owned collection must have at least one record. Otherwise it is treated as common, until
    // someone performs write operation over.
    if (metainfo.empty()) {
        if (op.is_write()) {
            COCAINE_LOG_INFO(log, "initializing ACL for '{}' collection for uid(s) [{}]", collection,
                [&](std::ostream& stream) -> std::ostream& {
                    return stream << boost::join(uids | boost::adaptors::transformed(static_cast<std::string(*)(uid_t)>(std::to_string)), ", ");
                }
            );

            for (auto uid : uids) {
                metainfo[uid] = flags_t::both;
            }
            backend->put<metainfo_t>(defaults::collection_acls, collection, metainfo, {}).get();
        }
        return;
    }

    const auto allowed = std::all_of(std::begin(uids), std::end(uids), [&](uid_t uid) {
        return (metainfo[uid] & op.flag) == op.flag;
    });

    if (!allowed) {
        throw std::system_error(std::make_error_code(std::errc::permission_denied));
    }
}

auto
enabled_t::verify(std::size_t, const std::string&, const std::string&, const auth::identity_t&, callback_type)
    -> void
{
    BOOST_ASSERT("unimplemented, sorry");
}

} // namespace storage
} // namespace authorization
} // namespace cocaine
