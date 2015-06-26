#pragma once

#include <string>

#include <asio/local/stream_protocol.hpp>

namespace cocaine {

template<class To>
struct endpoint_traits;

template<>
struct endpoint_traits<asio::local::stream_protocol::endpoint> {
    static inline
    std::string
    path(const asio::local::stream_protocol::endpoint& ep) {
        auto path = boost::lexical_cast<std::string>(ep.path());
        auto it = path.find_last_of("/");
        if (it == std::string::npos) {
            return path;
        } else {
            return path.substr(it + 1);
        }
    }
};

template<>
struct endpoint_traits<asio::ip::tcp::endpoint> {
    static inline
    std::string
    path(const asio::ip::tcp::endpoint& ep) {
        return boost::lexical_cast<std::string>(ep);
    }

    template<class From>
    static inline
    asio::ip::tcp::endpoint
    cast(const From& endpoint) {
        switch (endpoint.protocol().family()) {
        case AF_INET: {
            const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(endpoint.data());
            asio::ip::address_v4::bytes_type array;
            std::copy((char*)&addr->sin_addr, (char*)&addr->sin_addr + array.size(), array.begin());
            asio::ip::address_v4 address(array);
            return asio::ip::tcp::endpoint(
                address,
                asio::detail::socket_ops::network_to_host_short(addr->sin_port)
            );
        }
        case AF_INET6: {
            const sockaddr_in6* addrv6 = reinterpret_cast<const sockaddr_in6*>(endpoint.data());
            asio::ip::address_v6::bytes_type array;
            std::copy((char*)&addrv6->sin6_addr, (char*)&addrv6->sin6_addr + array.size(), array.begin());
            asio::ip::address_v6 address(array, addrv6->sin6_scope_id);
            return asio::ip::tcp::endpoint(
                address,
                asio::detail::socket_ops::network_to_host_short(addrv6->sin6_port)
            );
        }
        default:
            BOOST_ASSERT(false);
        };

        return asio::ip::tcp::endpoint();
    }
};

template<>
struct endpoint_traits<std::string> {
    template<class From>
    static inline
    std::string
    cast(const From& endpoint) {
        // TODO: Это пиздец.
        switch (endpoint.protocol().family()) {
        case AF_INET:
        case AF_INET6:
            return boost::lexical_cast<std::string>(endpoint_traits<asio::ip::tcp::endpoint>::cast(endpoint));
        case AF_UNIX: {
            const sockaddr_un* addr = (const sockaddr_un*)(endpoint.data());
            return std::string(addr->sun_path);
        }
        default:
            break;
        };

        return "<unknown protocol type>";
    }
};

}
