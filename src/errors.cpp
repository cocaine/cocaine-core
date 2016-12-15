/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/errors.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/memory.hpp"

#include <asio/error.hpp>

#include <vector>

using namespace cocaine;
using namespace cocaine::error;

namespace {

class unknown_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "unknown category";
    }

    virtual
    auto
    message(int) const -> std::string {
        return "unknown category error";
    }
};

class transport_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.rpc.transport";
    }

    virtual
    auto
    message(int code) const -> std::string {
        if(code == cocaine::error::transport_errors::frame_format_error)
            return "message has an unexpected framing";
        if(code == cocaine::error::transport_errors::hpack_error)
            return "unable to decode message metadata";
        if(code == cocaine::error::transport_errors::insufficient_bytes)
            return "insufficient bytes provided to decode the message";
        if(code == cocaine::error::transport_errors::parse_error)
            return "unable to parse the incoming data";

        return "cocaine.rpc.transport error";
    }
};

class dispatch_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.rpc.dispatch";
    }

    virtual
    auto
    message(int code) const -> std::string {
        if(code == cocaine::error::dispatch_errors::duplicate_slot)
            return "duplicate slot";
        if(code == cocaine::error::dispatch_errors::invalid_argument)
            return "unable to decode message arguments";
        if(code == cocaine::error::dispatch_errors::not_connected)
            return "session is detached";
        if(code == cocaine::error::dispatch_errors::revoked_channel)
            return "specified channel was revoked";
        if(code == cocaine::error::dispatch_errors::slot_not_found)
            return "specified slot is not bound";
        if(code == cocaine::error::dispatch_errors::unbound_dispatch)
            return "no dispatch has been assigned for channel";
        if(code == cocaine::error::dispatch_errors::uncaught_error)
            return "uncaught invocation exception";

        return "cocaine.rpc.dispatch error";
    }
};

class repository_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.plugins";
    }

    virtual
    auto
    message(int code) const -> std::string {
        if(code == cocaine::error::repository_errors::component_not_found)
            return "component is not available";
        if(code == cocaine::error::repository_errors::duplicate_component)
            return "duplicate component";
        if(code == cocaine::error::repository_errors::initialization_error)
            return "component has failed to intialize";
        if(code == cocaine::error::repository_errors::invalid_interface)
            return "component has an invalid interface";
        if(code == cocaine::error::repository_errors::dlopen_error)
            return "internal libltdl error";
        if(code == cocaine::error::repository_errors::version_mismatch)
            return "component version requirements are not met";

        return "cocaine.plugins error";
    }
};

class security_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.security";
    }

    virtual
    auto
    message(int code) const -> std::string {
        if(code == cocaine::error::security_errors::token_not_found)
            return "specified token is not available";

        return "cocaine.security error";
    }
};

// Locator errors

struct locator_category_t:
public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.service.locator";
    }

    virtual
    auto
    message(int code) const -> std::string {
        switch(code) {
            case cocaine::error::locator_errors::service_not_available:
                return "service is not available";
            case cocaine::error::locator_errors::routing_storage_error:
                return "routing storage is unavailable";
            case cocaine::error::locator_errors::missing_version_error:
                return "missing protocol version";
            case cocaine::error::locator_errors::gateway_duplicate_service:
                return "duplicate service provided to gateway";
            case cocaine::error::locator_errors::gateway_missing_service:
                return "service is missing in gateway";
        }

        return "cocaine.service.locator error";
    }
};

class unicorn_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.plugins.unicorn";
    }

    virtual
    auto
    message(int code) const -> std::string {
        switch (code) {
            case child_not_allowed :
                return "can not get value of a node with childs";
            case invalid_type :
                return "invalid type of value stored for requested operation";
            case invalid_value :
                return "could not unserialize value stored in zookeeper";
            case unknown_error:
                return "unknown zookeeper error";
            case invalid_node_name:
                return "inavlid node name specified";
            case invalid_path:
                return "inavlid path specified";
            case version_not_allowed:
                return "specified version is not allowed for command";
            default:
                return std::string("Unknown unicorn error - ") + std::to_string(code);
        }
    }
};

auto
unknown_category() -> const std::error_category& {
    static unknown_category_t instance;
    return instance;
}

auto
transport_category() -> const std::error_category& {
    static transport_category_t instance;
    return instance;
}

auto
dispatch_category() -> const std::error_category& {
    static dispatch_category_t instance;
    return instance;
}

auto
repository_category() -> const std::error_category& {
    static repository_category_t instance;
    return instance;
}

auto
security_category() -> const std::error_category& {
    static security_category_t instance;
    return instance;
}

auto
unicorn_category() -> const std::error_category& {
    static unicorn_category_t instance;
    return instance;
}

auto
locator_category() -> const std::error_category& {
    static locator_category_t instance;
    return instance;
}

} // namespace

namespace cocaine { namespace error {

auto
make_error_code(transport_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), transport_category());
}

auto
make_error_code(dispatch_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), dispatch_category());
}

auto
make_error_code(repository_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), repository_category());
}

auto
make_error_code(security_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), security_category());
}

auto
make_error_code(locator_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), locator_category());
}

auto
make_error_code(unicorn_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), unicorn_category());
}

std::string
to_string(const std::system_error& e) {
    return cocaine::format("[{}] {}", e.code().value(), e.what());
}

const std::error_code
error_t::kInvalidArgumentErrorCode = std::make_error_code(std::errc::invalid_argument);

}} // namespace cocaine::error

// Error category registrar

namespace {
struct category_record_t {
    size_t id;
    const std::error_category* category;
};
}
class registrar::storage_type {
public:
    typedef std::vector<category_record_t> mapping_t;

    synchronized<mapping_t> mapping;

    storage_type() {
        mapping.unsafe() = mapping_t({
            {0x01, &std::system_category()              },
            {0x02, &asio::error::get_system_category()  },
            {0x03, &asio::error::get_netdb_category()   },
            {0x04, &asio::error::get_addrinfo_category()},
            {0x05, &asio::error::get_misc_category()    },
            {0x06, &transport_category()                },
            {0x07, &dispatch_category()                 },
            {0x08, &repository_category()               },
            {0x09, &security_category()                 },
            {0x0A, &locator_category()                  },
            {0x0B, &unicorn_category()                  },
            {0x0C, &std::generic_category()             },
            {0xFF, &unknown_category()                  }
        });
    }
};

auto
registrar::add(const std::error_category& ec, size_t index) -> void {
    instance().mapping.apply([&](storage_type::mapping_t& mapping){
        auto comp = [=](const category_record_t& item) {
            return item.id == index;
        };
        auto it = std::find_if(mapping.begin(), mapping.end(), comp);
        if(it == mapping.end()){
            mapping.push_back(category_record_t{index, &ec});
        } else if(std::strcmp(it->category->name(), ec.name()) != 0) {
            throw error_t("duplicate error category '{}' for index {}, already have '{}'", ec.name(), index, it->category->name());
        }
    });
}

auto
registrar::map(const std::error_category& ec) -> size_t {
    return instance().mapping.apply([&](storage_type::mapping_t& mapping) ->size_t {
        auto comp = [&](const category_record_t& item) {
            return strcmp(item.category->name(), ec.name()) == 0;
        };
        auto it = std::find_if(mapping.begin(), mapping.end(), comp);
        if(it == mapping.end()) {
            return 0xFF;
        } else {
            return it->id;
        }
    });
}

auto
registrar::map(size_t id) -> const std::error_category& {
    return instance().mapping.apply([&](storage_type::mapping_t& mapping) -> const std::error_category& {
        auto comp = [=](const category_record_t& item) {
            return item.id == id;
        };
        auto it = std::find_if(mapping.begin(), mapping.end(), comp);
        if(it == mapping.end()) {
            return unknown_category();
        } else {
            return *it->category;
        }
    });
}

auto registrar::instance() -> storage_type& {
    static storage_type self;
    return self;
}
