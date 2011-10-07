#ifndef COCAINE_THREADING_HPP
#define COCAINE_THREADING_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/security/digest.hpp"

namespace cocaine { namespace engine {

// Driver types
#define AUTO        1
#define MANUAL      2
#define FILESYSTEM  3
#define SINK        4
#define SERVER      5

// Thread manager
class overseer_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<overseer_t>
{
    public:
        overseer_t(zmq::context_t& context, const helpers::auto_uuid_t& engine_id);
       
        // Thread entry point 
        void run(boost::shared_ptr<plugin::source_t> source);
        
    public:
        // Driver bindings
        inline std::string id() const { return m_id.get(); }
        inline ev::dynamic_loop& loop() { return m_loop; }
        inline zmq::context_t& context() { return m_context; }
        inline boost::shared_ptr<plugin::source_t> source() { return m_source; }
        inline lines::channel_t& link() { return m_link; }

    private:
        // Event loop callback handling and dispatching
        void request(ev::io& w, int revents);
        void timeout(ev::timer& w, int revents);

        // Thread request handling
        template<class DriverType>
        Json::Value push(const Json::Value& args);
        
        Json::Value drop(const std::string& driver_id);
        
        void terminate();

    private:
        // Thread ID
        helpers::auto_uuid_t m_id;

        // Messaging
        zmq::context_t& m_context;
        lines::channel_t m_link;
        
        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_request;
        ev::timer m_timeout;
        
        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
        
        // Driver management (Driver ID -> Driver)
        typedef boost::ptr_map<const std::string, drivers::abstract_driver_t> slave_map_t;
        slave_map_t m_slaves;

        // Storage key generation
        security::digest_t m_digest;
};

}}

#endif
