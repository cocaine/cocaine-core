#pragma once

#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "cocaine/auth/uid.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/rpc/protocol.hpp"

namespace cocaine {
namespace api {
namespace authorization {

class storage_t {
public:
    typedef storage_t category_type;
    typedef std::function<void(std::error_code)> callback_type;

    virtual
    ~storage_t() = default;

    template<typename Event>
    auto
    verify(const std::string& collection, const std::string& key, const auth::identity_t& identity) -> void {
        verify(io::event_traits<Event>::id, collection, key, identity);
    }

    /// Verifies access to a collection for the given identity.
    ///
    /// An implementation is meant to match the underlying access rights with users. An exception
    /// must be thrown on any unauthorized access as like as access mismatch.
    ///
    /// \overload
    /// \param event Event to identify access rights.
    /// \param collection Collection to operate with.
    /// \param key Key to operate with.
    /// \param identity Identity of a user or users ithat wish to operate with the resource.
    virtual
    auto
    verify(std::size_t event, const std::string& collection, const std::string& key, const auth::identity_t& identity)
        -> void = 0;

    virtual
    auto
    verify(std::size_t event, const std::string& collection, const std::string& key, const auth::identity_t& identity, callback_type callback)
        -> void = 0;
};

auto
storage(context_t& context, const std::string& service) -> std::shared_ptr<storage_t>;

} // namespace authorization
} // namespace api
} // namespace cocaine
