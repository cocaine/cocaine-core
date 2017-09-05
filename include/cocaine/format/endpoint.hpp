#pragma once

#include "cocaine/format/base.hpp"

#include <asio/ip/basic_endpoint.hpp>

namespace cocaine {

template<class Protocol>
struct display<asio::ip::basic_endpoint<Protocol>> {
    using value_type = asio::ip::basic_endpoint<Protocol>;

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        return stream << value.address() << ":" << value.port();
    }
};

template<class Protocol>
struct display_traits<asio::ip::basic_endpoint<Protocol>> : public string_display<asio::ip::basic_endpoint<Protocol>> {};

} // namespace cocaine
