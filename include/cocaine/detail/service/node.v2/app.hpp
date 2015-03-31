#pragma once

#include "cocaine/context.hpp"

namespace cocaine { namespace service { namespace v2 {

class app_t {
public:
    app_t(context_t& context, const std::string& name, const std::string& profile);

    void start();
};

}}} // namespace cocaine::service::v2
