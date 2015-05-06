#pragma once

#include <system_error>

#include <boost/variant/variant.hpp>

namespace cocaine {

template<class T>
class result : public boost::variant<T, std::error_code> {
public:
    result(T ok) : boost::variant<T, std::error_code>(std::move(ok)) {}
    result(std::error_code err) : boost::variant<T, std::error_code>(std::move(err)) {}
};

}
