#include <chrono>
#include <map>

#include <blackhole/logger.hpp>

#include "cocaine/api/authorization/storage.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/locked_ptr.hpp"
#include "storage/metainfo.hpp"

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
