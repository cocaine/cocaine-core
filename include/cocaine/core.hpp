//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/auth.hpp"
#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace core {

class core_t:
    public boost::noncopyable
{
    public:
        core_t(const config_t& config);
        ~core_t();

        void loop();
        
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
        
        // User request handling
        Json::Value create_engine(const std::string& name, 
                                  const Json::Value& manifest, 
                                  bool recovering = false);

        Json::Value reload_engine(const std::string& name);
        Json::Value delete_engine(const std::string& name);

        Json::Value info() const;

        // Responding
        bool respond(const networking::route_t& route, const Json::Value& object);

        // Task recovering
        void recover();

        // Automatic discovery support
        void announce(ev::timer&, int);

    private:
        context_t m_context;
        crypto::auth_t m_auth;
        
        networking::socket_t m_server;

        ev::sig m_sigint, m_sigterm, m_sigquit, m_sighup;
        ev::io m_watcher;
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

#if BOOST_VERSION >= 104000
        typedef boost::ptr_unordered_map<
#else
        typedef boost::ptr_map<
#endif
            const std::string,
            engine::engine_t
        > engine_map_t;

        engine_map_t m_engines;

        // Automatic discovery support
        boost::shared_ptr<networking::socket_t> m_announces;
        boost::shared_ptr<ev::timer> m_announce_timer;

        // Uptime
        ev::tstamp m_birthstamp;
};

}}

#endif
