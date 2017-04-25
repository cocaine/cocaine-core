#pragma once

#include <chrono>
#include <map>
#include <tuple>

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
        std::map<auth::cid_t, authorization::storage::flags_t>,
        std::map<auth::uid_t, authorization::storage::flags_t>
    > underlying_type;

    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& target, const authorization::storage::metainfo_t& source) {
        type_traits<underlying_type>::pack(target, source.c_perms, source.u_perms);
    }

    static
    void
    unpack(const msgpack::object& source, authorization::storage::metainfo_t& target) {
        type_traits<underlying_type>::unpack(source, target.c_perms, target.u_perms);
    }
};

} // namespace io
} // namespace cocaine
