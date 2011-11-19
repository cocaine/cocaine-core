#ifndef COCAINE_NETWORKING_HPP
#define COCAINE_NETWORKING_HPP

#include <msgpack.hpp>

#include "cocaine/common.hpp"

// Message types
#define INVOKE      1    /* engine -> worker: do something*/
#define TERMINATE   2    /* engine -> worker: die */

#define CHUNK       10   /* worker -> engine: something is in progress, here's the part of the result */
#define CHOKE       11   /* worker -> engine: something is done, choke the channel */
#define ERROR       12   /* worker -> engine: something is broken, choke the channel */
#define SUICIDE     13   /* worker -> engine: i am useless, kill me */
#define HEARTBEAT   14   /* worker -> engine: i am alive, don't kill me */

namespace cocaine { namespace lines {

using namespace boost::tuples;

typedef std::vector<std::string> route_t;

class socket_t: 
    public boost::noncopyable,
    public birth_control_t<socket_t>
{
    public:
        socket_t(zmq::context_t& context, int type, std::string route = "");

        void bind(const std::string& endpoint);
        void connect(const std::string& endpoint);
       
        bool send(zmq::message_t& message, int flags = 0);
        bool recv(zmq::message_t* message, int flags = 0);

        void getsockopt(int name, void* value, size_t* length);
        void setsockopt(int name, const void* value, size_t length);

        int fd();

        inline std::string endpoint() const { 
            return m_endpoint; 
        }

        inline std::string route() const { 
            return m_route; 
        }

        bool pending(int event = ZMQ_POLLIN);
        bool more();
#if ZMQ_VERSION > 30000
        bool label();
#endif

    private:
        zmq::socket_t m_socket;
        std::string m_endpoint, m_route;
};

template<class T> class raw;

template<class T>
inline static raw<T> protect(T& object) {
    return raw<T>(object);
}

template<class T>
inline static raw<const T> protect(const T& object) {
    return raw<const T>(object);
}

template<> class raw<std::string> {
    public:
        raw(std::string& object):
            m_object(object)
        { }

        inline void pack(zmq::message_t& message) const {
            message.rebuild(m_object.length());
            memcpy(message.data(), m_object.data(), m_object.length());
        }

        inline bool unpack(/* const */ zmq::message_t& message) {
            m_object.assign(
                static_cast<const char*>(message.data()),
                message.size());
            return true;
        }

    private:
        std::string& m_object;
};

template<> class raw<const std::string> {
    public:
        raw(const std::string& object):
            m_object(object)
        { }

        inline void pack(zmq::message_t& message) const {
            message.rebuild(m_object.length());
            memcpy(message.data(), m_object.data(), m_object.length());
        }

    private:
        const std::string& m_object;
};

class channel_t:
    public socket_t
{
    public:
        channel_t(zmq::context_t& context, int type, std::string identity = ""):
            socket_t(context, type, identity)
        { }

        // Bring original methods into the scope
        using socket_t::send;
        using socket_t::recv;

        // Packs and sends a single object
        template<class T>
        bool send(const T& value, int flags = 0) {
            zmq::message_t message;
            
            msgpack::sbuffer buffer;
            msgpack::pack(buffer, value);
            
            message.rebuild(buffer.size());
            memcpy(message.data(), buffer.data(), buffer.size());
            
            return send(message, flags);
        }

        template<class T>
        inline bool send(const raw<T>& object, int flags) {
            zmq::message_t message;
            object.pack(message);
            return send(message, flags);
        }

        // Packs and sends a tuple
        inline bool send_multi(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head>
        inline bool send_multi(const cons<Head, null_type>& o, int flags = 0) {
            return send(o.get_head(), flags);
        }

        template<class Head, class Tail>
        inline bool send_multi(const cons<Head, Tail>& o, int flags = 0) {
            return (send(o.get_head(), ZMQ_SNDMORE | flags) 
                    && send_multi(o.get_tail(), flags));
        }

        // Receives and unpacks a single object
        template<class T>
        bool recv(T& result, int flags = 0) {
            zmq::message_t message;
            msgpack::unpacked unpacked;

            if(!recv(&message, flags)) {
                return false;
            }
           
            try { 
                msgpack::unpack(&unpacked,
                    static_cast<const char*>(message.data()),
                    message.size());
                msgpack::object object = unpacked.get();
                object.convert(&result);
            // TODO: Figure out the msgpack exception type
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "net: [%s()] corrupted object - %s", __func__, e.what());
                return false;
            }

            return true;
        }
      
        template<class T>
        inline bool recv(raw<T>& result, int flags) {
            zmq::message_t message;

            if(!recv(&message, flags)) {
                return false;
            }

            return result.unpack(message);
        }

        // Receives and unpacks a tuple
        inline bool recv_multi(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head, class Tail>
        inline bool recv_multi(cons<Head, Tail>& o, int flags = 0) {
            return (recv(o.get_head(), flags)
                    && recv_multi(o.get_tail(), flags));
        }
        
        // Drops the current message
        inline void ignore() {
            zmq::message_t null;

            while(more()) {
                recv(&null);
            }
        }
};

}}

#endif
