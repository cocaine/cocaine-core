#ifndef YAPPI_DRIVERS_HPP
#define YAPPI_DRIVERS_HPP

#include <boost/format.hpp>

#include "common.hpp"
#include "networking.hpp"

#define max(a, b) ((a) >= (b) ? (a) : (b))
#define min(a, b) ((a) <= (b) ? (a) : (b))

namespace yappi { namespace engine { 
    
namespace threading {
    class overseer_t;
}

namespace drivers {

class abstract_t {
    public:
        virtual ~abstract_t() {};
};

template<class WatcherType, class DriverType>
class driver_base_t:
    public boost::noncopyable,
    public abstract_t
{
    public:
        driver_base_t(boost::shared_ptr<plugin::source_t> source);
        virtual ~driver_base_t();

        inline std::string id() const { return m_id; }

        void start(zmq::context_t& context, threading::overseer_t* parent);
        void stop();

        virtual void operator()(WatcherType&, int);

    protected:
        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
        
        // Driver ID
        std::string m_id;

        // Parent
        threading::overseer_t* m_parent;
        
        // Messaging
        std::auto_ptr<net::msgpack_socket_t> m_pipe;
        
        // Watcher
        std::auto_ptr<WatcherType> m_watcher;
};

class fs_t:
    public driver_base_t<ev::stat, fs_t>
{
    public:
        fs_t(boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            driver_base_t<ev::stat, fs_t>(source),
            m_path(args.get("path", "").asString())
        {
            if(m_path.empty()) {
                throw std::runtime_error("no path specified");
            }

            m_id = (boost::format("fs:%1%@%2%") % source->hash() % m_path).str();
        }

        inline void initialize() {
            m_watcher->set(m_path.c_str());
        }

    private:
        const std::string m_path;
};

template<class TimedDriverType>
class timed_driver_base_t:
    public driver_base_t<ev::periodic, timed_driver_base_t<TimedDriverType> >
{
    public:
        timed_driver_base_t(boost::shared_ptr<plugin::source_t> source):
            driver_base_t<ev::periodic, timed_driver_base_t>(source)
        {}

        inline void initialize() {
            ev_periodic_set(static_cast<ev_periodic*>(this->m_watcher.get()), 0, 0, thunk);
        }

    private:
        inline ev::tstamp reschedule(ev::tstamp now) {
            return static_cast<TimedDriverType*>(this)->reschedule(now);
        }

        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now);
};

class auto_t:
    public timed_driver_base_t<auto_t>
{
    public:
        auto_t(boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            timed_driver_base_t<auto_t>(source),
            m_interval(args.get("interval", 0).asInt() / 1000.0)
        {
            if(m_interval <= 0) {
                throw std::runtime_error("no interval specified");
            }

            m_id = (boost::format("auto:%1%@%2%") % source->hash() % m_interval).str();
        }
       
        inline ev::tstamp reschedule(ev::tstamp now) {
            return now + m_interval;
        }

    private:
        ev::tstamp m_interval;
};

class manual_t:
    public timed_driver_base_t<manual_t>
{
    public:
        manual_t(boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            timed_driver_base_t<manual_t>(source)
        {
            if(!(m_source->capabilities() & CAP_MANUAL)) {
                throw std::runtime_error("source doesn't support manual scheduling");
            }
            
            m_id = "manual:" + m_source->hash();
        }

        inline ev::tstamp reschedule(ev::tstamp now) {
            return max(now, m_source->reschedule());
        }
};

class event_t:
    public driver_base_t<ev::io, event_t>
{
    public:
        event_t(boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            driver_base_t<ev::io, event_t>(source),
            m_endpoint(args.get("endpoint", "").asString())
        {
            if(!(m_source->capabilities() & CAP_SINK)) {
                throw std::runtime_error("source doesn't support incoming messages");
            }

            if(m_endpoint.empty()) {
                throw std::runtime_error("no endpoint specified");
            }

            m_id = (boost::format("event:%1%@%2%") % m_source->hash() % m_endpoint).str();
        }

        virtual void operator()(ev::io&, int) {
            // Do something
        }

        void initialize() {
            m_sink.reset(new net::json_socket_t(m_parent->context(), ZMQ_PULL));
            m_sink->bind(m_endpoint);
            
            m_watcher->set(m_sink->fd(), EV_READ);
        }

    private:
        std::string m_endpoint;
        std::auto_ptr<net::blob_socket_t> m_sink;
};

}}}

#endif
