#include "cocaine/api/authorization/storage.hpp"

#include "cocaine/forwards.hpp"

#include <blackhole/logger.hpp>

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

public:
    enabled_t(context_t& context, const std::string& service, const dynamic_t& args);

    auto
    verify(std::size_t event, const std::string& collection, const std::string& key, const auth::identity_t& identity, callback_type callback)
        -> void override;

private:
    auto
    verify(std::size_t event, const std::string& collection, const std::vector<auth::uid_t>& uids, callback_type callback)
        -> void;
};

} // namespace storage
} // namespace authorization
} // namespace cocaine
