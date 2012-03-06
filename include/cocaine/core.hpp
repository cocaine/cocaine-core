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

#ifndef COCAINE_CORE_HPP
#define COCAINE_CORE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

#include "cocaine/networking.hpp"

namespace cocaine { namespace core {

class core_t:
    public object_t
{
    public:
        core_t(context_t& ctx);
        ~core_t();

        void loop();

        // User request handling
        Json::Value create_engine(const std::string& name, 
                                  const Json::Value& manifest, 
                                  bool recovering = false);
        
        Json::Value delete_engine(const std::string& name);

        Json::Value info() const;
        
    private:
        // Signal processing
        void terminate(ev::sig&, int);
        void reload(ev::sig&, int);

        // User request processing
        void request(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);

        // User request dispatching
        Json::Value dispatch(const Json::Value& root);
        
        // Task recovering
        void recover();

        // Automatic discovery support
        void announce(ev::timer&, int);

    private:
        // Uptime
        const ev::tstamp m_birthstamp;
        
        // Engines
#if BOOST_VERSION >= 104000
        typedef boost::ptr_unordered_map<
#else
        typedef boost::ptr_map<
#endif
            const std::string,
            engine::engine_t
        > engine_map_t;

        engine_map_t m_engines;

        // Event watchers
        ev::sig m_sigint, m_sigterm, m_sigquit, m_sighup;
        ev::io m_watcher;
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        // System I/O 
        networking::socket_t m_server;

        // Automatic discovery support
        boost::shared_ptr<ev::timer> m_announce_timer;
        boost::shared_ptr<networking::socket_t> m_announces;
};

}}

#endif
