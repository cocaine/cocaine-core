#pragma once

#include "cocaine/api/authorization/storage.hpp"
#include "cocaine/api/authorization/unicorn.hpp"
#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

namespace cocaine {
namespace api {

template<typename C>
struct make_category {
    typedef std::shared_ptr<C> ptr_type;

    struct factory_type : public basic_factory<C> {
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

template<>
struct category_traits<authorization::storage_t> : public make_category<authorization::storage_t> {};

template<>
struct category_traits<authorization::unicorn_t> : public make_category<authorization::unicorn_t> {};

} // namespace api
} // namespace cocaine
