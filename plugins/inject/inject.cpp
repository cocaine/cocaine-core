#include <stdexcept>

#include <zmq.hpp>

#include "plugin.hpp"
#include "uri.hpp"

namespace yappi { namespace plugin {

zmq::context_t* g_context;

class inject_t:
    public source_t
{
    public:
        inject_t(const std::string& uri_):
            source_t(uri_),
            m_socket(*g_context, ZMQ_PULL)
        {
            yappi::helpers::uri_t uri(uri_);
            std::string path = "/var/run/yappi/" + uri.host();

            try {
                m_socket.bind(("ipc://" + path).c_str());
            } catch(const zmq::error_t& e) {
                throw std::runtime_error("cannot bind on " + path + " - " + e.what());
            }
        }
    
        virtual uint32_t capabilities() const {
            return ITERATOR;
        }

        virtual dict_t invoke() {
            dict_t dict;
            zmq::message_t message;
            std::string key, value;

            while(m_socket.recv(&message, ZMQ_NOBLOCK)) {
                key.assign(
                    reinterpret_cast<char*>(message.data()),
                    message.size());

                m_socket.recv(&message);

                value.assign(
                    reinterpret_cast<char*>(message.data()),
                    message.size());

                dict[key] = value;
            }
            
            return dict;
        }

    private:
        zmq::socket_t m_socket;
};

source_t* create_inject_instance(const char* uri) {
    return new inject_t(uri);
}

static const plugin_info_t plugin_info = {
    1,
    {
        { "inject", &create_inject_instance }
    }
};

extern "C" {
    const plugin_info_t* initialize() {
        g_context = new zmq::context_t(1);        
        return &plugin_info;
    }

    __attribute__((destructor)) void destructor() {
        delete g_context;
    }
}

}}
