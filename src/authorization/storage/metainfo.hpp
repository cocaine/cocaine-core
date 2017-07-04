#pragma once

#include <chrono>
#include <map>
#include <string>
#include <tuple>

#include <boost/lexical_cast.hpp>

#include "cocaine/traits/enum.hpp"
#include "cocaine/traits/map.hpp"
#include "cocaine/traits/tuple.hpp"

namespace cocaine {
namespace authorization {
namespace storage {

enum flags_t : std::size_t { none = 0x00, read = 0x01, write = 0x02, both = read | write };

struct metainfo_t {
    std::map<auth::cid_t, flags_t> c_perms;
    std::map<auth::uid_t, flags_t> u_perms;

    auto
    empty() const -> bool {
        return c_perms.empty() && u_perms.empty();
    }
};

// Used as dirty workaround to unify metainfo pack/unpack as unicorn uses
// dynamic_t as a value, which can't handle (yet) strings as a dictionary keys.
struct metainfo_persistent_stub_t {
    std::map<std::string, flags_t> c_perms;
    std::map<std::string, flags_t> u_perms;
};

namespace detail {
    template<typename T, typename DstMap>
    auto stringify_keys(const std::map<T, flags_t>& src, DstMap& dst) -> void {
        for(const auto& el : src) {
            dst[boost::lexical_cast<std::string>(el.first)] = el.second;
        }
    }

    template<typename T, typename SrcMap>
    auto digitize_keys(const SrcMap& src, std::map<T, flags_t>& dst) -> void {
        for(const auto& el : src) {
            dst[boost::lexical_cast<T>(el.first)] = el.second;
        }
    }
}

// Placed here as static interface to avoid creation of 'metainfo.cpp'
struct stub_transform {
    static
    auto stub_from_meta(const metainfo_t& meta) -> metainfo_persistent_stub_t {
       metainfo_persistent_stub_t stub;
       detail::stringify_keys<auth::cid_t>(meta.c_perms, stub.c_perms);
       detail::stringify_keys<auth::uid_t>(meta.u_perms, stub.u_perms);
       return stub;
    }

    static
    auto meta_from_stub(const metainfo_persistent_stub_t& stub) -> metainfo_t {
        metainfo_t meta;
        detail::digitize_keys<auth::cid_t>(stub.c_perms, meta.c_perms);
        detail::digitize_keys<auth::uid_t>(stub.u_perms, meta.u_perms);
        return meta;
    }

    stub_transform() = delete;
};

template<typename T>
class cached {
public:
    using value_type = T;
    using clock_type = std::chrono::system_clock;

    enum class tag_t { actual, expired };

private:
    value_type value;
    clock_type::time_point expiration_time;

public:
    cached(value_type value, clock_type::duration duration) :
        value(std::move(value)),
        expiration_time(clock_type::now() + duration)
    {}

    auto
    is_expired() const -> bool {
        return clock_type::now() > expiration_time;
    }

    auto
    get() const -> std::tuple<tag_t, value_type> {
        auto tag = tag_t::actual;
        if (is_expired()) {
            tag = tag_t::expired;
        }

        return std::make_tuple(tag, value);
    }
};

} // namespace storage
} // namespace authorization
} // namespace cocaine

namespace cocaine {
namespace io {

template<>
struct type_traits<authorization::storage::metainfo_t> {
    typedef boost::mpl::list<
        std::map<std::string, authorization::storage::flags_t>,
        std::map<std::string, authorization::storage::flags_t>
    > underlying_type;

    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& target, const authorization::storage::metainfo_t& source) {
        namespace as = authorization::storage;
        const auto stub = as::stub_transform::stub_from_meta(source);
        type_traits<underlying_type>::pack(target, stub.c_perms, stub.u_perms);
    }

    static
    void
    unpack(const msgpack::object& source, authorization::storage::metainfo_t& target) {
        namespace as = authorization::storage;
        auto stub = as::metainfo_persistent_stub_t{ {}, {} };
        type_traits<underlying_type>::unpack(source, stub.c_perms, stub.u_perms);
        target = as::stub_transform::meta_from_stub(stub);
    }
};

} // namespace io
} // namespace cocaine
