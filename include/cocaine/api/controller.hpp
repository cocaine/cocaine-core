#pragma once

#include <memory>
#include <string>
#include <vector>

#include "cocaine/api/auth.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/repository.hpp"
#include "cocaine/rpc/protocol.hpp"

namespace cocaine {
namespace api {
namespace controller {

class event_t {
public:
    typedef event_t category_type;

    template<typename Event>
    auto
    verify(const std::vector<uid_t>& uids) -> void {
        verify(Event::alias(), uids);
    }

    virtual
    auto
    verify(const std::string& event, const std::vector<uid_t>& uids) -> void = 0;
};

class collection_t {
public:
    typedef collection_t category_type;

    template<typename Event>
    auto
    verify(const std::string& collection, const std::string& key, const std::vector<uid_t>& uids) -> void {
        verify(io::event_traits<Event>::id, collection, key, uids);
    }

    /// Verifies access to a collection for given users.
    ///
    /// An implementation is meant to match the underlying access rights with users. An exception
    /// must be thrown on any unauthorized access as like as access mismatch.
    ///
    /// \param event Event to identify access rights.
    /// \param collection Collection to operate with.
    /// \param key Key to operate with.
    /// \param uids List of users that wish to operate with resource.
    virtual
    auto
    verify(std::size_t event, const std::string& collection, const std::string& key, const std::vector<uid_t>& uids)
        -> void = 0;
};

auto
event(context_t& context, const std::string& service) -> std::shared_ptr<event_t>;

auto
collection(context_t& context, const std::string& service) -> std::shared_ptr<collection_t>;

} // namespace controller
} // namespace api
} // namespace cocaine
