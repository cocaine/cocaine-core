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

#ifndef COCAINE_NATIVE_SERVER_DRIVER_HPP
#define COCAINE_NATIVE_SERVER_DRIVER_HPP

#include "cocaine/common.hpp"

// Has to be included after common.h
#include <ev++.h>

#include "cocaine/io.hpp"
#include "cocaine/job.hpp"

#include "cocaine/interfaces/driver.hpp"

namespace cocaine { namespace engine { namespace drivers {

class native_server_t:
    public driver_t
{
    public:
        typedef driver_t category_type;

    public:
        native_server_t(context_t& context,
                        engine_t& engine,
                        const plugin_config_t& config);

        ~native_server_t();

        // Driver interface.
        virtual Json::Value info() const;
        
    private:
        typedef boost::tuple<
            std::string&,
            policy_t&,
            zmq::message_t*
        > request_proxy_t;

        void event(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        const std::string m_event,
                          m_route;

        ev::io m_watcher;
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        // Server RPC channel.        
        io::channel_t m_channel;
};

}}}

#endif
