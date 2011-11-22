#ifndef COCAINE_OVERSEER_HPP
#define COCAINE_OVERSEER_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace engine {

// Thread manager
class overseer_t:
    public boost::noncopyable,
    public unique_id_t
{
    public:
        overseer_t(unique_id_t::reference id,
                   zmq::context_t& context,
                   const std::string& name);
        ~overseer_t();

        // Entry point 
        void operator()(const std::string& type, const std::string& args);

        // Callback used to send response chunks
        void respond(const void* response, size_t size);

    private:
        // Event loop callback handling and dispatching
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void timeout(ev::timer&, int);
        void heartbeat(ev::timer&, int);

        void terminate();

    private:
        // Messaging
        zmq::context_t& m_context;
        lines::channel_t m_messages;

        // Application instance
        std::string m_app_name;
        boost::shared_ptr<plugin::source_t> m_app;

        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_watcher;
        ev::idle m_processor;
        ev::timer m_suicide_timer, m_heartbeat_timer;
};

}}

#endif
