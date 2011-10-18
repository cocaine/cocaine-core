#ifndef COCAINE_CORE_HPP
#define COCAINE_CORE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/responses.hpp"
#include "cocaine/security/signatures.hpp"

namespace cocaine { namespace core {

class core_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<core_t>,
    public lines::responder_t
{
    public:
        core_t();
        ~core_t();

        // Event loop
        void run();
        
    private:
        // Signal processing
        void terminate(ev::sig& sig, int revents);
        void reload(ev::sig& sig, int revents);

        // User request processing
        void request(ev::io& io, int revents);
        void process_request(ev::idle& w, int revents);

        // User request dispatching
        void dispatch(boost::shared_ptr<lines::response_t> response, const Json::Value& root);
        
        // User request handling
        Json::Value create_engine(const Json::Value& manifest);
        Json::Value delete_engine(const Json::Value& manifest);
        Json::Value stats();

        // Future support
        virtual void respond(const lines::route_t& route, const Json::Value& object);

        // Task recovering
        void recover();

    private:
        security::signatures_t m_signatures;

        // Networking
        zmq::context_t m_context;
        lines::socket_t m_server;

        // Event watchers
        ev::sig m_sigint, m_sigterm, m_sigquit, m_sighup;
        ev::io m_request_watcher;
        ev::idle m_request_processor;

        // Engine management (URI -> Engine)
        typedef std::map<const std::string, boost::shared_ptr<engine::engine_t> > engine_map_t;
        engine_map_t m_engines;
};

}}

#endif
