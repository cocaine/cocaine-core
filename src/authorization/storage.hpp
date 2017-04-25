#include <chrono>
#include <map>

#include <blackhole/logger.hpp>

#include "cocaine/api/authorization/storage.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/locked_ptr.hpp"

namespace cocaine {
namespace authorization {
namespace storage {

class disabled_t : public api::authorization::storage_t {
public:
    disabled_t(context_t& context, const std::string& service, const dynamic_t& args);

    auto
    verify(std::size_t event, const std::string& collection, const std::string& key, const auth::identity_t& identity, callback_type callback)
        -> void override;
};

enum flags_t: std::size_t {
    none  = 0x00,
    read  = 0x01,
    write = 0x02,
    both  = read | write
};

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

    enum class tag_t {
        actual,
        expired
    };

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

class enabled_t : public api::authorization::storage_t {
    std::unique_ptr<logging::logger_t> log;
    std::shared_ptr<api::storage_t> backend;

    std::chrono::seconds cache_duration;
    synchronized<std::map<std::string, cached<metainfo_t>>> cache;

public:
    enabled_t(context_t& context, const std::string& service, const dynamic_t& args);

    auto
    verify(std::size_t event, const std::string& collection, const std::string& key, const auth::identity_t& identity, callback_type callback)
        -> void override;

private:
    auto
    verify(std::size_t event, const std::string& collection, const auth::identity_t& identity, callback_type callback)
        -> void;
};

} // namespace storage
} // namespace authorization
} // namespace cocaine
