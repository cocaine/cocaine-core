#include "runtime.hpp"

#include "cocaine/traits/dynamic.hpp"
#include "cocaine/traits/map.hpp"

namespace cocaine {
namespace service {

runtime_t::runtime_t(context_t& context,
                     asio::io_service& asio,
                     const std::string& name,
                     const dynamic_t& args) :
    api::service_t(context, asio, name, args),
    dispatch<io::runtime_tag>(name),
    hub(context.metrics_hub())
{
    on<io::runtime::metrics>([&]() -> dynamic_t {
        return metrics();
    });
}

auto runtime_t::metrics() const -> dynamic_t {
    dynamic_t::object_t result;

    auto counters = hub.counters<std::int64_t>();

    for (const auto& item : counters) {
        const auto& tags = std::get<0>(item);
        const auto& counter = std::get<1>(item);

        result[tags.name()] = counter.get()->load();
    }

    return result;
}

}  // namespace service
}  // namespace cocaine
