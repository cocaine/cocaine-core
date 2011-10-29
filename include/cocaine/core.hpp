#ifndef COCAINE_CORE_HPP
#define COCAINE_CORE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/security/signatures.hpp"

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
        void terminate(ev::sig& sig, int revents);
        void reload(ev::sig& sig, int revents);

        // User request processing
        void request(ev::io& io, int revents);
        void process(ev::idle& w, int revents);

        // User request dispatching
        Json::Value dispatch(const Json::Value& root);
        
        // User request handling
        Json::Value create_engine(const std::string& name, const Json::Value& manifest);
        Json::Value delete_engine(const std::string& name);
        Json::Value info();

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

        typedef std::map<
            const std::string,
            boost::shared_ptr<engine::engine_t>
        > engine_map_t;

        engine_map_t m_engines;
};

}}

#endif
