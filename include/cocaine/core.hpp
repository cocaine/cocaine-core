#ifndef COCAINE_CORE_HPP
#define COCAINE_CORE_HPP

#define EV_MINIMAL 0
#include <ev++.h>

#include "cocaine/common.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/security/signatures.hpp"

namespace cocaine { namespace core {

class future_t;

class core_t:
    public boost::noncopyable
{
    friend class future_t;
    
    public:
        core_t();
        ~core_t();

        // Event loop
        void run();
        
    private:
        // Signal processing
        void terminate(ev::sig& sig, int revents);
        void reload(ev::sig& sig, int revents);
        void purge(ev::sig& sig, int revents);

        // Request processing
        void request(ev::io& io, int revents);

        // Request dispatching
        void dispatch(future_t* future, const Json::Value& root);
        
        // Request handling
        void push(future_t* future, const std::string& target, const Json::Value& args);
        void drop(future_t* future, const std::string& target, const Json::Value& args);
        void history(future_t* future, const std::string& key, const Json::Value& args);
        void stat(future_t* future);

        // Future sealing
        void seal(const std::string& future_id);

        // Publishing
        void event(ev::io& io, int revents);

        // Engine request processing and dispatching
        void internal(ev::io& io, int revents);

        // Engine request handling
        void fulfill(const Json::Value& args);
        void reap(const Json::Value& args);
        
        // Task recovery
        void recover();

    private:
        security::signatures_t m_signatures;

        // Engine management (URI -> Engine)
        typedef boost::ptr_map<const std::string, engine::engine_t> engine_map_t;
        engine_map_t m_engines;

        // Future management
        typedef boost::ptr_map<const std::string, future_t> future_map_t;
        future_map_t m_futures;

        // History
        typedef std::deque< std::pair<ev::tstamp, plugin::dict_t> > history_t;
        typedef boost::ptr_map<std::string, history_t> history_map_t;
        history_map_t m_histories;

        // Networking
        zmq::context_t m_context;
        net::msgpack_socket_t s_events;
        net::blob_socket_t s_publisher;
        net::json_socket_t s_requests, s_internal;
        
        // Event loop
        ev::default_loop m_loop;
        ev::io e_events, e_requests, e_internal;
        ev::sig e_sigint, e_sigterm, e_sigquit, e_sighup, e_sigusr1;

        // Hostname
        std::string m_hostname;
};

}}

#endif
