#ifndef COCAINE_DRIVERS_HPP
#define COCAINE_DRIVERS_HPP

#include <boost/format.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/security/digest.hpp"

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

namespace cocaine { namespace engine { namespace drivers {

class abstract_driver_t {
    public:
        virtual ~abstract_driver_t() {};

        inline std::string id() const { return m_id; }

    protected:
        // Driver ID
        std::string m_id;
};

template<class WatcherType, class DriverType>
class driver_base_t:
    public boost::noncopyable,
    public abstract_driver_t
{
    public:
        driver_base_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source):
            m_parent(parent),
            m_source(source) 
        {}
        
        virtual ~driver_base_t() {
            if(m_watcher.get() && m_watcher->is_active()) {
                m_watcher->stop();
            }
        }

        void start() {
            m_pipe.reset(new net::msgpack_socket_t(m_parent->context(), ZMQ_PUSH));
            m_pipe->connect("inproc://events");
            
            m_watcher.reset(new WatcherType(m_parent->loop()));
            m_watcher->set(this);

            static_cast<DriverType*>(this)->initialize();

            m_watcher->start();
        }

        inline void stop() {
            m_parent->reap(m_id);
        }

        virtual void operator()(WatcherType&, int) {
            const plugin::dict_t& dict = m_parent->invoke();

            // Do nothing if plugin has returned an empty dict
            if(dict.size() == 0) {
                return;
            }

            publish(dict);
        }
    
    protected:
        void publish(const plugin::dict_t& dict) {
            m_pipe->send_object(m_id, ZMQ_SNDMORE);
            m_pipe->send_object(dict);
        }

    protected:
        // Parent
        threading::overseer_t* m_parent;
        
        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
        
        // Messaging
        std::auto_ptr<net::msgpack_socket_t> m_pipe;
        
        // Watcher
        std::auto_ptr<WatcherType> m_watcher;

        // Hasher
        security::digest_t m_digest;
};

class fs_t:
    public driver_base_t<ev::stat, fs_t>
{
    public:
        fs_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            driver_base_t<ev::stat, fs_t>(parent, source),
            m_path(args.get("path", "").asString())
        {
            if(~m_source->capabilities() & plugin::source_t::ITERATOR) {
                throw std::runtime_error("source doesn't support iteration");
            }
            
            if(m_path.empty()) {
                throw std::runtime_error("no path specified");
            }

            m_id = "fs:" + m_digest.get(source->uri() + m_path);
        }

        inline void initialize() {
            m_watcher->set(m_path.c_str());
        }

    private:
        const std::string m_path;
};

template<class TimedDriverType>
class timed_driver_t:
    public driver_base_t<ev::periodic, timed_driver_t<TimedDriverType> >
{
    public:
        typedef TimedDriverType Type;

        timed_driver_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source):
            driver_base_t<ev::periodic, timed_driver_t>(parent, source)
        {}

        inline void initialize() {
            ev_periodic_set(static_cast<ev_periodic*>(this->m_watcher.get()), 0, 0, thunk);
        }

    private:
        inline ev::tstamp reschedule(ev::tstamp now) {
            return static_cast<Type*>(this)->reschedule(now);
        }

        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now) {
            timed_driver_t<Type>* driver = static_cast<timed_driver_t<Type>*>(w->data);

            try {
                return driver->reschedule(now);
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "engine: %s driver is broken - %s",
                    driver->id().c_str(), e.what());
                driver->stop();
                return now;
            }
        }
};

class auto_t:
    public timed_driver_t<auto_t>
{
    public:
        auto_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            timed_driver_t<auto_t>(parent, source),
            m_interval(args.get("interval", 0).asInt() / 1000.0)
        {
            if(~m_source->capabilities() & plugin::source_t::ITERATOR) {
                throw std::runtime_error("source doesn't support iteration");
            }
            
            if(m_interval <= 0) {
                throw std::runtime_error("no interval specified");
            }

            m_id = "auto:" + m_digest.get((boost::format("%1%%2%") 
                % source->uri() % m_interval).str());
        }
       
        inline ev::tstamp reschedule(ev::tstamp now) {
            return now + m_interval;
        }

    private:
        ev::tstamp m_interval;
};

class manual_t:
    public timed_driver_t<manual_t>
{
    public:
        manual_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            timed_driver_t<manual_t>(parent, source)
        {
            if(~m_source->capabilities() & plugin::source_t::SCHEDULER) {
                throw std::runtime_error("source doesn't support manual scheduling");
            }
            
            m_id = "manual:" + m_digest.get(m_source->uri());
        }

        inline ev::tstamp reschedule(ev::tstamp now) {
            return MAX(now, m_source->reschedule());
        }
};

class event_t:
    public driver_base_t<ev::io, event_t>
{
    public:
        event_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            driver_base_t<ev::io, event_t>(parent, source),
            m_endpoint(args.get("endpoint", "").asString())
        {
            if(~m_source->capabilities() & plugin::source_t::PROCESSOR) {
                throw std::runtime_error("source doesn't support message processing");
            }

            if(m_endpoint.empty()) {
                throw std::runtime_error("no endpoint specified");
            }

            m_id = "event:" + m_digest.get(m_source->uri() + m_endpoint);
        }

        virtual void operator()(ev::io&, int) {
            zmq::message_t message;
            plugin::dict_t dict; 

            while(m_sink->pending()) {
                m_sink->recv(&message);

                try {
                    dict = m_source.get()->process(message.data(), message.size());
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "engine: %s driver is broken - %s",
                        m_id.c_str(), e.what());
                    stop();
                    return;
                }

                publish(dict);
            }
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
