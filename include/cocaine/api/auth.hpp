#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <system_error>

#include <boost/variant/variant.hpp>

#include "cocaine/forwards.hpp"

namespace cocaine {
namespace api {

using uid_t = std::uint64_t;

class auth_t {
public:
    typedef auth_t category_type;
    typedef std::chrono::system_clock clock_type;

    static constexpr uid_t superuser = std::numeric_limits<uid_t>::min();
    static constexpr uid_t anonymous = std::numeric_limits<uid_t>::max();

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

    struct allow_t {
        std::vector<uid_t> uids;
    };

    /// Result of permissions check for a given message.
    typedef boost::variant<
        /// Successful check.
        allow_t,
        /// Forbidden, go away.
        std::error_code
    > permission_t;

    typedef std::function<void(token_t token, const std::error_code& ec)> callback_type;

public:
    virtual
    ~auth_t() = default;

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

    /// Checks access rights for a given authorization entity represented with a token, i.e can it
    /// use the requested event of a given service.
    ///
    /// \param token Authorization token.
    /// \tparam Event Requested event.
    template<typename Event>
    auto
    check_permissions(const std::string& credentials) -> permission_t {
        return check_permissions(Event::alias(), credentials);
    }

    /// Checks access rights for a given authorization entity represented with a token, i.e can it
    /// use the requested event of a given service.
    ///
    /// \param event Requested event.
    /// \param token Authorization token.
    virtual
    auto
    check_permissions(const std::string& event, const std::string& credentials) const ->
        permission_t = 0;
};

auto
auth(context_t& context, const std::string& name, const std::string& service) ->
    std::shared_ptr<auth_t>;

}  // namespace api
}  // namespace cocaine
