#ifndef YAPPI_SCHEDULERS_HPP
#define YAPPI_SCHEDULERS_HPP

#include <boost/format.hpp>

#include "common.hpp"
#include "engine.hpp"

namespace yappi { namespace engine { namespace detail {

#define max(a, b) ((a) >= (b) ? (a) : (b))
#define min(a, b) ((a) <= (b) ? (a) : (b))

class overseer_t;

class scheduler_t {
    public:
        virtual ~scheduler_t() {};
};

template<class WatcherType, class SchedulerType>
class scheduler_base_t:
    public boost::noncopyable,
    public scheduler_t
{
    public:
        scheduler_base_t(boost::shared_ptr<plugin::source_t> source);
        virtual ~scheduler_base_t();

        inline std::string id() const { return m_id; }

        void start(zmq::context_t& context, overseer_t* parent);
        void stop();

        void operator()(WatcherType&, int);

    protected:
        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
        
        // Scheduler ID
        std::string m_id;

        // Parent
        overseer_t* m_parent;
        
        // Messaging
        std::auto_ptr<net::blob_socket_t> m_pipe;
        
        // Watcher
        std::auto_ptr<WatcherType> m_watcher;
};

class fs_scheduler_t:
    public scheduler_base_t<ev::stat, fs_scheduler_t>,
    public helpers::birth_control_t<fs_scheduler_t>
{
    public:
        fs_scheduler_t(boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            scheduler_base_t<ev::stat, fs_scheduler_t>(source),
            m_path(args.get("path", "").asString())
        {
            if(m_path.empty()) {
                throw std::runtime_error("invalid path");
            }

            m_id = (boost::format("fs:%1%@%2%") % source->hash() % m_path).str();
        }

        void initialize();

    private:
        const std::string m_path;
};

template<class TimedSchedulerType>
class timed_scheduler_base_t:
    public scheduler_base_t<ev::periodic, timed_scheduler_base_t<TimedSchedulerType> >
{
    public:
        timed_scheduler_base_t(boost::shared_ptr<plugin::source_t> source):
            scheduler_base_t<ev::periodic, timed_scheduler_base_t>(source)
        {}

        void initialize();

    private:
        inline ev::tstamp reschedule(ev::tstamp now) {
            return static_cast<TimedSchedulerType*>(this)->reschedule(now);
        }

        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now);
};

// Automatic scheduler
class auto_scheduler_t:
    public timed_scheduler_base_t<auto_scheduler_t>,
    public helpers::birth_control_t<auto_scheduler_t>    
{
    public:
        auto_scheduler_t(boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            timed_scheduler_base_t<auto_scheduler_t>(source),
            m_interval(args.get("interval", 0).asInt() / 1000.0)
        {
            if(m_interval <= 0) {
                throw std::runtime_error("invalid interval");
            }

            m_id = (boost::format("auto:%1%@%2%") % source->hash() % m_interval).str();
        }
       
        inline ev::tstamp reschedule(ev::tstamp now) {
            return now + m_interval;
        }

    private:
        ev::tstamp m_interval;
};

// Manual userscript scheduler
class manual_scheduler_t:
    public timed_scheduler_base_t<manual_scheduler_t>,
    public helpers::birth_control_t<manual_scheduler_t>
{
    public:
        manual_scheduler_t(boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            timed_scheduler_base_t<manual_scheduler_t>(source)
        {
            if(!(m_source->capabilities() & CAP_MANUAL)) {
                throw std::runtime_error("manual scheduling is not supported");
            }
            
            m_id = "manual:" + m_source->hash();
        }

        inline ev::tstamp reschedule(ev::tstamp now) {
            return max(now, m_source->reschedule());
        }
};

}}}

#endif
