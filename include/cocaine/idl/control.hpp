#pragma once

#include <system_error>

// primitive protocol is always required for responses on control events
#include "cocaine/idl/primitive.hpp"
#include "cocaine/rpc/tags.hpp"

namespace cocaine { namespace io {

template<class Tag>
struct messages;

template<class Tag>
struct protocol;

struct control_tag;

struct control {

/// Allows for immediate termination of a stream.
struct revoke {
    typedef control_tag tag;

    static const char* alias() {
        return "revoke";
    }

    typedef boost::mpl::list<
        /// Channel to be revoked.
        std::uint64_t,
        /// Error reason for channel revocation.
        std::error_code
    >::type argument_type;
};

/// The settings event conveys configuration parameters that affect how endpoints communicate,
/// such as preferences and constraints on peer behavior.
///
/// This is also used to acknowledge the receipt of those parameters.
struct settings {
    typedef control_tag tag;

    static const char* alias() {
        return "settings";
    }
};

/// The ping event is a mechanism for measuring a minimal round-trip time from the sender, as well
/// as determining whether an idle connection is still functional.
struct ping {
    typedef control_tag tag;

    static const char* alias() {
        return "ping";
    }
};

/// The goaway event is used to initiate shutdown of a connection or to signal serious error
/// conditions.
///
/// It allows an endpoint to gracefully stop accepting new streams while still
/// finishing processing of previously established streams. This enables administrative actions,
/// like server maintenance.
struct goaway {
    typedef control_tag tag;

    static const char* alias() {
        return "goaway";
    }

    typedef boost::mpl::list<
        /// Error reason.
        std::error_code
    >::type argument_type;
};

}; // struct control

template<>
struct protocol<control_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        control::revoke,
        control::settings,
        control::ping,
        control::goaway
        // TODO: To be added more, incomplete.
    >::type messages;

    typedef control scope;
};

}} // namespace cocaine::io
