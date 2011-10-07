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
        friend class future_t;

        core_t();
        ~core_t();

        // Event loop
        void run();
        
        // Publishing
        void event(const std::string& driver_id, const Json::Value& result);
        
    private:
        // Signal processing
        void terminate(ev::sig& sig, int revents);
        void reload(ev::sig& sig, int revents);
        void purge(ev::sig& sig, int revents);

        // User request processing
        void request(ev::io& io, int revents);

        // User request dispatching
        void dispatch(future_t* future, const Json::Value& root);
        
        // User request handling
        void push(future_t* future, const std::string& target, const Json::Value& args);
        void drop(future_t* future, const std::string& target, const Json::Value& args);
        void past(future_t* future, const std::string& target, const Json::Value& args);
        void stat(future_t* future);

        // Responding
        void seal(const std::string& future_id);

        // Task recovering
        void recover();

    private:
        security::signatures_t m_signatures;

        // Engine management (URI -> Engine)
        typedef boost::ptr_map<const std::string, engine::engine_t> engine_map_t;
        engine_map_t m_engines;

        // Future management (Future ID -> Future)
        typedef boost::ptr_map<const std::string, future_t> future_map_t;
        future_map_t m_futures;

        // History (Driver ID -> History List)
        typedef std::deque< std::pair<ev::tstamp, Json::Value> > history_t;
        typedef boost::ptr_map<const std::string, history_t> history_map_t;
        history_map_t m_histories;

        // Networking
        zmq::context_t m_context;
        lines::socket_t s_requests, s_publisher;
        
        // Event loop
        ev::default_loop m_loop;
        ev::io e_requests;
        ev::sig e_sigint, e_sigterm, e_sigquit, e_sighup, e_sigusr1;

        // Hostname
        std::string m_hostname;
};

}}

#endif
