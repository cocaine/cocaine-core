//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef COCAINE_RPC_HPP
#define COCAINE_RPC_HPP

#include "cocaine/events.hpp"
#include "cocaine/job.hpp"

namespace cocaine { namespace engine { namespace rpc {
    enum codes {
        heartbeat,
        terminate,
        invoke,
        push,
        error,
        release
    };

    // Specialize this class for any new event types.
    template<typename T> struct pack;

    template<>
    struct pack<events::heartbeat_t> {
        boost::tuple<int> get() const {
            return boost::make_tuple(heartbeat);
        }
    };

    template<>
    struct pack<events::terminate_t> {
        boost::tuple<int> get() const {
            return boost::make_tuple(terminate);
        }
    };

    template<>
    struct pack<events::invoke_t> {
        pack(const boost::shared_ptr<job_t>& job):
            method(job->method()),
            message(job->request().data(), 
                    job->request().size(), 
                    NULL)
        { }

        boost::tuple<int, const std::string&, zmq::message_t&> get() {
            return boost::tuple<int, const std::string&, zmq::message_t&>(invoke, method, message);
        }

        const std::string method;
        zmq::message_t message;
    };

    template<>
    struct pack<events::push_t> {
        pack(const void * data, size_t size):
            message(size)
        {
            memcpy(message.data(), data, size);
        }

        boost::tuple<int, zmq::message_t&> get() {
            return boost::tuple<int, zmq::message_t&>(push, message);
        }

        zmq::message_t message;
    };

    template<>
    struct pack<events::error_t> {
        pack(const std::string& message):
            code(client::server_error),
            message(message)
        { }

        pack(const std::runtime_error& e):
            code(client::server_error),
            message(e.what())
        { }

        pack(const recoverable_error_t& e):
            code(client::app_error),
            message(e.what())
        { }

        pack(const unrecoverable_error_t& e):
            code(client::server_error),
            message(e.what())
        { }

        boost::tuple<int, unsigned int, const std::string&> get() const {
            return boost::tuple<int, unsigned int, const std::string&>(error, code, message);
        }

        const unsigned int code;
        const std::string message;
    };

    template<>
    struct pack<events::release_t> {
        boost::tuple<int> get() const {
            return boost::make_tuple(release);
        }        
    };
}}}

#endif

