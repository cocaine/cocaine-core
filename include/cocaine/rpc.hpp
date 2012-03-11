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

#include "cocaine/job.hpp"

#include "cocaine/dealer/types.hpp"

namespace cocaine { namespace engine { namespace rpc {
    enum codes {
        heartbeat,
        terminate,
        invoke,
        push,
        error,
        release
    };

    template<codes>
    struct command;

    template<>
    struct command<heartbeat> {
        boost::tuple<int> get() {
            return boost::make_tuple(heartbeat);
        }
    };

    template<>
    struct command<terminate> {
        boost::tuple<int> get() {
            return boost::make_tuple(terminate);
        }
    };

    template<>
    struct command<invoke> {
        command(const boost::shared_ptr<job_t>& job):
            method(job->method()),
            message(job->request().data(), job->request().size(), NULL)
        { }

        boost::tuple<int, const std::string&, zmq::message_t&> get() {
            return boost::tuple<int, const std::string&, zmq::message_t&>(invoke, method, message);
        }

        const std::string method;
        zmq::message_t message;
    };

    template<>
    struct command<push> {
        command(const void * data, size_t size):
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
    struct command<error> {
        command(const std::string& message):
            code(client::server_error),
            message(message)
        { }

        command(const std::runtime_error& e):
            code(client::server_error),
            message(e.what())
        { }

        command(const recoverable_error_t& e):
            code(client::app_error),
            message(e.what())
        { }

        command(const unrecoverable_error_t& e):
            code(client::server_error),
            message(e.what())
        { }

        boost::tuple<int, unsigned int, const std::string&> get() {
            return boost::tuple<int, unsigned int, const std::string&>(error, code, message);
        }

        const unsigned int code;
        const std::string message;
    };

    template<>
    struct command<release> {
        boost::tuple<int> get() {
            return boost::make_tuple(release);
        }
    };
}}}

#endif

