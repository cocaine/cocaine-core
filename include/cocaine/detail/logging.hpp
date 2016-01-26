#pragma once

#include <string>
#include <typeinfo>

namespace cocaine { namespace detail { namespace logging {

// C++ typename demangling

auto
demangle(const std::string& mangled) -> std::string;

template<class T>
auto
demangle() -> std::string {
    return demangle(typeid(T).name());
}

}}} // namespace cocaine::detail::logging
