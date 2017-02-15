#include "cocaine/api/controller.hpp"

#include "cocaine/forwards.hpp"

#include <blackhole/logger.hpp>

namespace cocaine {
namespace controller {
namespace collection {

using uid_t = api::uid_t;

class null_t : public api::controller::collection_t {
public:
    null_t(context_t& context, const std::string& service, const dynamic_t& args);

    auto
    verify(std::size_t event, const std::string& collection, const std::string& key, const std::vector<uid_t>& uids)
        -> void override;
};

class control_t : public api::controller::collection_t {
    std::unique_ptr<logging::logger_t> log;
    std::shared_ptr<api::storage_t> backend;

public:
    control_t(context_t& context, const std::string& service, const dynamic_t& args);

    auto
    verify(std::size_t event, const std::string& collection, const std::string& key, const std::vector<uid_t>& uids)
        -> void override;

private:
    auto
    verify(std::size_t event, const std::string& collection, const std::vector<uid_t>& uids)
        -> void;
};

} // namespace collection
} // namespace controller
} // namespace cocaine
