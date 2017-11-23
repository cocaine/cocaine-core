#pragma once

#include <cocaine/dynamic.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format/dynamic.hpp>

#include <asio/ip/tcp.hpp>

namespace cocaine {

template<>
struct dynamic_converter<asio::ip::tcp::endpoint> {
    typedef asio::ip::tcp::endpoint result_type;

    static const bool enable = true;

    static inline
    result_type
    convert(const dynamic_t& from) {
        if (!from.is_array() || from.as_array().size() != 2) {
            throw error_t("invalid dynamic value for endpoint deserialization {}", from);
        }
        auto ep_pair = from.as_array();
        if (!ep_pair[0].is_string() || !ep_pair[1].is_uint()) {
            throw error_t("invalid dynamic value for endpoint deserialization", from);
        }
        std::string host = ep_pair[0].to<std::string>();
        unsigned short int port = ep_pair[1].to<unsigned short int>();
        asio::ip::tcp::endpoint result(asio::ip::address::from_string(host), port);
        return result;
    }

    static inline
    bool
    convertible(const dynamic_t& from) {
        return from.is_array() &&
               from.as_array().size() == 2 &&
               from.as_array()[0].is_string() &&
               (from.as_array()[1].is_uint() || from.as_array()[1].is_int());
    }
};


} // namespace cocaine
