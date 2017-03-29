#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <system_error>

#include <boost/variant/variant.hpp>

#include "cocaine/auth/uid.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/hpack/header.hpp"

namespace cocaine {
namespace api {

/// Identification and autentification interface.
class authentication_t {
public:
    typedef authentication_t category_type;
    typedef std::chrono::system_clock clock_type;

    struct token_t {
        /// Token type. Think of the first part of the Authorization HTTP header.
        std::string type;

        /// Token body.
        std::string body;

        /// Time point when the token becomes expired. Note that due to NTP misconfiguration,
        /// slow network of whatever else the token may become expired in unexpected way. Use this
        /// information as a cache hint.
        clock_type::time_point expires_in;

        token_t() = default;

        /// Returns true if the token is expired.
        auto
        expired() const -> bool {
            return clock_type::now() >= expires_in;
        }
    };

    typedef std::function<void(token_t token, const std::error_code& ec)> callback_type;

public:
    virtual
    ~authentication_t() = default;

    /// Tries to obtain, possibly asynchronously, an authorization token with its type and optional
    /// expiration time point.
    ///
    /// The implementation is free to cache obtained token.
    ///
    /// \param callback The function which is called on either when the token is read or any error.
    ///     It's strongly recommended to wrap the callback with an executor to avoid deadlocks or
    ///     inner threads context switching.
    virtual
    auto
    token(callback_type callback) -> void = 0;

    /// Extracts an authorization header from the given headers with its further identification
    /// check.
    ///
    /// \tparam Event Requested event.
    /// \param headers HPACK headers.
    virtual
    auto
    identify(const hpack::headers_t& headers) const -> auth::identity_t;

    /// Performs an identification check for a given entity represented with a token.
    ///
    /// \param event Requested event.
    /// \param token Identification credentials.
    virtual
    auto
    identify(const std::string& credentials) const -> auth::identity_t = 0;
};

auto
authentication(context_t& context, const std::string& name, const std::string& service) ->
    std::shared_ptr<authentication_t>;

} // namespace api
} // namespace cocaine
