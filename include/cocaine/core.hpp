#ifndef COCAINE_CORE_HPP
#define COCAINE_CORE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/lines.hpp"
#include "cocaine/security.hpp"

namespace cocaine { namespace core {

class core_t:
    public boost::noncopyable
{
    public:
        core_t();
        ~core_t();

        void start();
        
    private:
        // Signal processing
        void terminate(ev::sig&, int);
        void reload(ev::sig&, int);

        // User request processing
        void request(ev::io&, int);
        void process(ev::idle&, int);

        // User request dispatching
        Json::Value dispatch(const Json::Value& root);
        
        // User request handling
        Json::Value create_engine(const std::string& name, 
                                  const Json::Value& manifest, 
                                  bool recovering = false);

        Json::Value delete_engine(const std::string& name);
        Json::Value info() const;

        // Responding
        void respond(const lines::route_t& route, const Json::Value& object);

        // Task recovering
        void recover();

    private:
        security::signatures_t m_signatures;

        zmq::context_t m_context;
        lines::socket_t m_server;

        ev::sig m_sigint, m_sigterm, m_sigquit, m_sighup;
        ev::io m_watcher;
        ev::idle m_processor;

        typedef boost::ptr_unordered_map<
            const std::string,
            engine::engine_t
        > engine_map_t;

        engine_map_t m_engines;
};

}}

#endif
