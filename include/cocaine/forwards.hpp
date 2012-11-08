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
    // Common configuration.
    struct config_t;

    // Runtime context.
    class context_t;
    
    // App configuration.
    struct manifest_t;
    struct profile_t;

    namespace rpc {
        struct rpc_plane_tag;
    }

    namespace control {
        struct control_plane_tag;
    }

    // App container.
    class app_t;

    namespace engine {
        // Queueing modes.
        enum mode: int {
            normal,
            blocking
        };

        // Execution engine.
        class engine_t;
        class slave_t;
        
        // Event abstractions.
        struct event_t;
        struct session_t;

        // Session control pipe.
        struct pipe_t;
    }

    namespace io {
        namespace policies {
            struct unique;
            struct shared;
        }

        // RPC channel.
        template<class, class>
        class channel;
    }

    namespace logging {
        // Logging sink abstraction.
        class sink_t;

        // Logging proxy.
        class logger_t;
    }

    struct unique_id_t;
}

namespace zmq {
    class context_t;
    class message_t;
}

#endif
