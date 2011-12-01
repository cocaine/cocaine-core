#include "cocaine/lines.hpp"

using namespace cocaine::lines;

socket_t::socket_t(zmq::context_t& context, int type, std::string route):
    m_socket(context, type),
    m_route(route)
{
    if(!route.empty()) {
        setsockopt(ZMQ_IDENTITY, route.data(), route.size());
    } 
}

void socket_t::bind(const std::string& endpoint) {
    m_socket.bind(endpoint.c_str());

    // Try to determine the connection string for clients
    // TODO: Do it the right way
    size_t position = endpoint.find_last_of(":");

    if(position != std::string::npos) {
        m_endpoint = config_t::get().core.hostname +
            endpoint.substr(position, std::string::npos);
    } else {
        m_endpoint = "<local>";
    }
}

void socket_t::connect(const std::string& endpoint) {
    m_socket.connect(endpoint.c_str());
}
       
bool socket_t::send(zmq::message_t& message, int flags) {
    try {
        return m_socket.send(message, flags);
    } catch(const zmq::error_t& e) {
        syslog(LOG_ERR, "net: [%s()] %s", __func__, e.what());
        return false;
    }
}

bool socket_t::recv(zmq::message_t* message, int flags) {
    try {
        return m_socket.recv(message, flags);
    } catch(const zmq::error_t& e) {
        syslog(LOG_ERR, "net: [%s()] %s", __func__, e.what());
        return false;
    }
}

void socket_t::drop_remaining_parts() {
    zmq::message_t null;

    while(more()) {
        recv(&null);
    }
}

void socket_t::getsockopt(int name, void* value, size_t* size) {
    m_socket.getsockopt(name, value, size);
}

void socket_t::setsockopt(int name, const void* value, size_t size) {
    m_socket.setsockopt(name, value, size);
}

int socket_t::fd() {
    int fd;
    size_t size = sizeof(fd);

    getsockopt(ZMQ_FD, &fd, &size);

    return fd;
}

bool socket_t::pending(int event) {
#if ZMQ_VERSION > 30000
    int events;
#else
    unsigned long events;
#endif

    size_t size = sizeof(events);

    getsockopt(ZMQ_EVENTS, &events, &size);

    return events & event;
}

bool socket_t::more() {
#if ZMQ_VERSION > 30000
    int rcvmore;
#else
    int64_t rcvmore;
#endif

    size_t size = sizeof(rcvmore);

    getsockopt(ZMQ_RCVMORE, &rcvmore, &size);

    return rcvmore != 0;
}

#if ZMQ_VERSION > 30000
bool socket_t::label() {
    int rcvlabel;
    size_t size = sizeof(rcvlabel);

    getsockopt(ZMQ_RCVLABEL, &rcvlabel, &size);

    return rcvlabel != 0;
}
#endif

