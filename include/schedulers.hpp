#ifndef YAPPI_SCHEDULERS_HPP
#define YAPPI_SCHEDULERS_HPP

#include <boost/format.hpp>

#include "common.hpp"
#include "engine.hpp"

namespace yappi { namespace engine { namespace {

#define max(a, b) ((a) >= (b) ? (a) : (b))
#define min(a, b) ((a) <= (b) ? (a) : (b))

class scheduler_base_t: public boost::noncopyable {
    public:
        scheduler_base_t(zmq::context_t& context, plugin::source_t& source,
            overseer_t& overseer);
        virtual ~scheduler_base_t();
        
        inline std::string id() const { return m_id; }
        
        void start();
        inline void stop() { m_stopping = true; }

    protected:
        virtual ev::tstamp reschedule(ev::tstamp now) = 0;

    private:
        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now);
        void publish(ev::periodic& w, int revents);
        
    protected:
        // Data source
        plugin::source_t& m_source;
        
        // Subscription key
        std::string m_id;

    private:
        // Messaging
        net::blob_socket_t m_uplink;
        
        // Parent
        overseer_t& m_overseer;
        
        // Watcher
        std::auto_ptr<ev::periodic> m_watcher;
        
        // Termination flag
        bool m_stopping;
};

// Automatic scheduler
class auto_scheduler_t: public scheduler_base_t {
    public:
        auto_scheduler_t(
            zmq::context_t& context, plugin::source_t& source,
            overseer_t& overseer, const Json::Value& args
        ):
            scheduler_base_t(context, source, overseer),
            m_interval(args.get("interval", 0).asInt() / 1000.0)
        {
            if(m_interval <= 0) {
                throw std::runtime_error("invalid interval");
            }

            m_id = (boost::format("auto:%1%@%2%") % source.hash() % m_interval).str();
        }
       
        virtual inline ev::tstamp reschedule(ev::tstamp now) {
            return now + m_interval;
        }

    private:
        ev::tstamp m_interval;
};

// Manual userscript scheduler
class manual_scheduler_t: public scheduler_base_t {
    public:
        manual_scheduler_t(
            zmq::context_t& context, plugin::source_t& source,
            overseer_t& overseer, const Json::Value& args
        ):
            scheduler_base_t(context, source, overseer) 
        {
            if(!(m_source.capabilities() & CAP_MANUAL)) {
                throw std::runtime_error("manual scheduling is not supported");
            }
            
            m_id = "manual:" + m_source.hash();
        }

        virtual inline ev::tstamp reschedule(ev::tstamp now) {
            return max(now, m_source.reschedule(now));
        }
};

}}}

#endif
