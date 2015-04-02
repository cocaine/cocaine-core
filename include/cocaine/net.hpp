#pragma once

#include <string>

namespace cocaine {

template<class To>
struct endpoint_traits;

template<>
struct endpoint_traits<std::string> {
    template<class From>
    static inline
    std::string
    cast(const From& endpoint) {
        // TODO: Это пиздец.
        switch (endpoint.protocol().family()) {
        case AF_INET: {
            const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(endpoint.data());
            asio::ip::address_v4::bytes_type array;
            std::copy((char*)&addr->sin_addr, (char*)&addr->sin_addr + array.size(), array.begin());
            asio::ip::address_v4 address(array);
            return boost::lexical_cast<std::string>(
                asio::ip::tcp::endpoint(
                    address,
                    asio::detail::socket_ops::network_to_host_short(addr->sin_port)
                )
            );
        }
        case AF_INET6: {
            const sockaddr_in6* addrv6 = reinterpret_cast<const sockaddr_in6*>(endpoint.data());
            asio::ip::address_v6::bytes_type array;
            std::copy((char*)&addrv6->sin6_addr, (char*)&addrv6->sin6_addr + array.size(), array.begin());
            asio::ip::address_v6 address(array, addrv6->sin6_scope_id);
            return boost::lexical_cast<std::string>(
                asio::ip::tcp::endpoint(
                    address,
                    asio::detail::socket_ops::network_to_host_short(addrv6->sin6_port)
                )
            );
        }
        case AF_UNIX: {
            const sockaddr_un* addr = (const sockaddr_un*)(endpoint.data());
            return std::string(addr->sun_path, addr->sun_len);
        }
        default:
            break;
        };

        return "<unknown protocol type>";
    }
};

}
