#pragma once

#include "cocaine/api/controller.hpp"
#include "cocaine/common.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/repository.hpp"

namespace cocaine {
namespace api {

template<>
struct category_traits<controller::collection_t> {
    typedef std::shared_ptr<controller::collection_t> ptr_type;

    struct factory_type : public basic_factory<controller::collection_t> {
        virtual
        auto
        get(context_t& context, const std::string& name, const std::string& service, const dynamic_t& args) -> ptr_type = 0;
    };

    template<class T>
    struct default_factory : public factory_type {
        auto
        get(context_t& context, const std::string&, const std::string& service, const dynamic_t& args) -> ptr_type override {
            return std::make_shared<T>(context, service, args);
        }
    };
};

}  // namespace api
}  // namespace cocaine
