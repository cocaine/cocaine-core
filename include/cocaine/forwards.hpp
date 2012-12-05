/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_FORWARDS_HPP
#define COCAINE_FORWARDS_HPP

namespace cocaine {
    // Runtime context.
    class context_t;
    
    // App configuration.
    struct manifest_t;
    struct profile_t;

    // App container.
    class app_t;

    namespace api {
        class driver_t;
        class isolate_t;
        class logger_t;
        class sandbox_t;
        class service_t;
        class storage_t;

        struct event_t;
        struct stream_t;
    }

    namespace engine {        
        // Unit of execution.
        struct session_t;

        // Execution queueing modes.
        enum mode: int {
            normal,
            blocking
        };

        // Execution engine.
        class engine_t;
        class slave_t;
    }

    namespace io {
        namespace tags {
            struct rpc_tag;
            struct control_tag;
        }

        namespace policies {
            struct unique;
            struct shared;
        }

        // RPC channel.
        template<class, class>
        class channel;
    }

    namespace logging {
        enum priorities: int {
            ignore,
            error,
            warning,
            info,
            debug
        };

        struct log_t;
    }

    struct unique_id_t;
}

namespace zmq {
    class context_t;
    class message_t;
}

#endif
