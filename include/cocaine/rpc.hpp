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

#include <boost/mpl/map.hpp>

#include "cocaine/events.hpp"
#include "cocaine/job.hpp"

namespace cocaine { namespace engine { namespace rpc {    
    
enum codes {
    heartbeat,
    configure,
    terminate,
    invoke,
    push,
    error,
    release
};

// Generic packer
// --------------

using namespace boost::mpl;

typedef map<
    pair< events::heartbeat_t, int_<heartbeat> >,
    pair< events::terminate_t, int_<terminate> >,
    pair< events::release_t,   int_<release  > >
> codemap;

template<typename T> 
struct packed {
    typedef boost::tuple<int> type;

    packed():
        pack(typename at<codemap, T>::type())
    { }

    type& get() {
        return pack;
    }

private:
    type pack;
};

// Specific packers
// ----------------

template<>
struct packed<events::configure_t> {
    typedef boost::tuple<int, const config_t&> type;

    packed(const events::configure_t& event):
        pack(configure, event.config)
    { }

    type& get() {
        return pack;
    }

private:
    type pack;
};

template<>
struct packed<events::invoke_t> {
    typedef boost::tuple<int, std::string, zmq::message_t&> type;

    packed(const events::invoke_t& event):
        message(event.job->request().size()),
        pack(invoke, event.job->method(), message)
    {
        memcpy(
            message.data(),
            event.job->request().data(),
            event.job->request().size()
        );
    }

    type& get() {
        return pack;
    }

private:
    zmq::message_t message;
    type pack;
};

template<>
struct packed<events::push_t> {
    typedef boost::tuple<int, zmq::message_t&> type;

    packed(const events::push_t& event):
        pack(push, message)
    {
        message.copy(&event.message);
    }

    type& get() {
        return pack;
    }

private:
    zmq::message_t message;
    type pack;
};

template<>
struct packed<events::error_t> {
    typedef boost::tuple<int, int, std::string> type;

    packed(const events::error_t& event):
        pack(error, event.code, event.message)
    { }

    type& get() {
        return pack;
    }

private:
    type pack;
};
    
}}}

#endif
