#ifndef YAPPI_SCHEDULERS_HPP
#define YAPPI_SCHEDULERS_HPP

#include <boost/format.hpp>

#include "common.hpp"
#include "engine.hpp"

namespace yappi { namespace engine { namespace {

class scheduler_base_t: public boost::noncopyable {
    public:
        scheduler_base_t(zmq::context_t& context, plugin::source_t& source,
            overseer_t& overseer);
        virtual ~scheduler_base_t();
        
        void start();
        inline std::string key() const { return m_key; }

    protected:
        virtual ev::tstamp reschedule(ev::tstamp now) = 0;

    private:
        inline static ev::tstamp thunk(ev_periodic* w, ev::tstamp now) {
            return static_cast<scheduler_base_t*>(w->data)->reschedule(now);
        }
        
        void publish(ev::periodic& w, int revents);
        
    protected:
        // Messaging
        net::blob_socket_t m_uplink;
        
        // Data source
        plugin::source_t& m_source;
        
        // Subscription key
        std::string m_key;

    private:
        // Parent
        overseer_t& m_overseer;
        
        // Watcher
        std::auto_ptr<ev::periodic> m_watcher;
};

// Automatic scheduler
class auto_scheduler_t: public scheduler_base_t {
    public:
        auto_scheduler_t(
            zmq::context_t& context, plugin::source_t& source,
            overseer_t& overseer, const Json::Value& args
        ):
            scheduler_base_t(context, source, overseer),
            m_interval(args.get("interval", 0).asInt())
        {
            if(m_interval <= 0) {
                throw std::runtime_error("invalid interval");
            }

            m_key = (boost::format("auto:%1%@%2%")
                % m_source.hash() % m_interval).str();
        }
       
        inline ev::tstamp reschedule(ev::tstamp now) {
            return now + m_interval / 1000.0;
        }

    private:
        time_t m_interval;
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
            m_key = "manual:" + m_source.hash();
        }

        inline ev::tstamp reschedule(ev::tstamp now) {
            // return m_source.reschedule(now);
            return now + 5.;
        }
};

}}}

#endif
