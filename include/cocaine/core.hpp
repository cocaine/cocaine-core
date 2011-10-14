#ifndef COCAINE_CORE_HPP
#define COCAINE_CORE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/security/signatures.hpp"

namespace cocaine { namespace core {

class core_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<core_t>
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
        void purge(ev::sig& sig, int revents);

        // User request processing
        void request(ev::io& io, int revents);

        // User request dispatching
        void dispatch(boost::shared_ptr<response_t> response, const Json::Value& root);
        
        // User request handling
        Json::Value create_engine(const Json::Value& manifest);
        Json::Value delete_engine(const Json::Value& manifest);
        Json::Value stats();

        // Task recovering
        void recover();

    private:
        security::signatures_t m_signatures;

        // Engine management (URI -> Engine)
        typedef std::map<const std::string, boost::shared_ptr<engine::engine_t> > engine_map_t;
        engine_map_t m_engines;

        // Networking
        zmq::context_t m_context;
        boost::shared_ptr<lines::socket_t> m_request;
        
        // Event loop
        ev::io m_request_watcher;
        ev::sig m_sigint, m_sigterm, m_sigquit, m_sighup, m_sigusr1;
};

}}

#endif
