#ifndef COCAINE_THREADING_HPP
#define COCAINE_THREADING_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/security/digest.hpp"

namespace cocaine { namespace engine {

// Thread manager
class overseer_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<overseer_t>,
    public helpers::unique_id_t
{
    public:
        overseer_t(helpers::unique_id_t::type id,
                   helpers::unique_id_t::type engine_id,
                   zmq::context_t& context);
       
        // Thread entry point 
        void run(boost::shared_ptr<plugin::source_t> source);
        
    public:
        // Driver bindings
        inline ev::dynamic_loop& loop() { return m_loop; }
        inline zmq::context_t& context() { return m_context; }
        inline boost::shared_ptr<plugin::source_t> source() { return m_source; }
        inline lines::channel_t& channel() { return m_channel; }
        inline bool isolated() const { return m_isolated; }

    private:
        // Event loop callback handling and dispatching
        void request(ev::io& w, int revents);
        void timeout(ev::timer& w, int revents);
        void heartbeat(ev::timer& w, int revents);

        // Thread request handling
        template<class DriverType>
        Json::Value push(const Json::Value& args);
        
        Json::Value drop(const std::string& driver_id);
        
        void terminate();

    private:
        // Messaging
        zmq::context_t& m_context;
        lines::channel_t m_channel;
        
        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_request;
        ev::timer m_timeout, m_heartbeat;

        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
        
        // Driver management (Driver ID -> Driver)
        typedef boost::ptr_map<const std::string, drivers::abstract_driver_t> slave_map_t;
        slave_map_t m_slaves;

        // Storage key generation
        security::digest_t m_digest;

        // Whether this thread is not the default one
        bool m_isolated;
};

}}

#endif
