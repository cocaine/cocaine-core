#ifndef YAPPI_CLIENT_HPP
#define YAPPI_CLIENT_HPP

#include <string>
#include <sstream>
#include <vector>
#include <map>

#include <boost/thread.hpp>
#include <zmq.hpp>

namespace yappi { namespace client {

class client_t;

// Generic interface
class consumer_base_t {
    public:
        consumer_base_t(client_t& parent, const std::string& field);
        inline std::string uuid() { return m_uuid; }
        
        virtual void dispatch(const std::string& source, const std::string& payload, time_t timestamp) = 0;

        bool subscribe(const std::vector<std::string>& urls);
        void unsubscribe();

    private:
        client_t& m_parent;
        std::string m_field;
        std::string m_uuid;
};

// Templated facade
template<class Container, class T = std::string>
class consumer_t: public consumer_base_t {
    public:
        consumer_t(client_t& parent, const std::string& field):
            consumer_base_t(parent, field)
        {}

        inline virtual void dispatch(
            const std::string& source,
            const std::string& payload,
            time_t timestamp)
        {
            std::istringstream xtr(payload);
            T message;

            xtr >> message;

            static_cast<Container*>(this)->consume(source, message, timestamp);
        }
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

}} // namespace yappi::client

#endif
