#ifndef COCAINE_CLIENT_HPP
#define COCAINE_CLIENT_HPP

#include <boost/thread.hpp>
#include <boost/function.hpp>

#include <zmq.hpp>

#include "common.hpp"

namespace cocaine { namespace client {

class client_t;

// Generic interface
class consumer_base_t {
    public:
        consumer_base_t(client_t& parent);
        virtual ~consumer_base_t();
        
        bool subscribe(const std::string& url, const std::string& field);
        bool subscribe(const std::vector<std::string>& urls, const std::string& field);

        virtual void dispatch(const std::string& source, const std::string& payload, time_t timestamp) = 0;
        
        inline std::string uuid() { return m_uuid; }

    private:
        client_t& m_parent;
        std::string m_uuid;
};

// Templated facade
template<class T = std::string>
class consumer_t: public consumer_base_t {
    public:
        typedef boost::function<void(const std::string&, T, time_t)> callback_t;

        consumer_t(client_t& parent, callback_t callback):
            consumer_base_t(parent),
            m_callback(callback)
        {}

        inline virtual void dispatch(
            const std::string& source,
            const std::string& payload,
            time_t timestamp)
        {
            std::istringstream xtr(payload);
            T message;

            xtr >> message;
            m_callback(source, message, timestamp);
        }

    private:
        callback_t m_callback;
};

// The client
class client_t {
    public:
        client_t();
        ~client_t();

        void connect(const std::string& control, const std::string& sink);

    protected:
        friend class consumer_base_t;

        bool subscribe(
            const std::vector<std::string>& urls,
            const std::string& field,
            consumer_base_t* consumer);

        void unsubscribe(const std::string& uuid);

    private:
        void processor();

    private:
        // Dispatch map
        typedef std::multimap<const std::pair<std::string, std::string>, consumer_base_t*> dispatch_map_t;
        dispatch_map_t m_dispatch;
       
        // Key->Source relationship
        std::map<std::string, std::string> m_sources;

        // 0MQ
        zmq::context_t m_context;
        zmq::socket_t m_control, m_sink, m_pipe;

        // Threading
        boost::thread* m_thread;
        boost::mutex m_mutex;
};

}} // namespace cocaine::client

#endif
