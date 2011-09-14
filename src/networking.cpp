#include "cocaine/networking.hpp"

using namespace cocaine::net;

bool json_socket_t::send_json(const Json::Value& root, int flags) {
    Json::FastWriter writer;

    std::string response = writer.write(root);
    zmq::message_t message(response.length());
    memcpy(message.data(), response.data(), response.length());

    return blob_socket_t::send(message, flags);
}

bool json_socket_t::recv_json(Json::Value& root, int flags) {
    Json::Reader reader(Json::Features::strictMode());
    zmq::message_t message;
    std::string request;

    if(!blob_socket_t::recv(&message, flags)) {
        return false;
    }

    request.assign(
        static_cast<const char*>(message.data()),
        message.size());

    if(!reader.parse(request, root)) {
        syslog(LOG_ERR, "net: invalid json - %s",
            reader.getFormatedErrorMessages().c_str());
        return false;
    }

    return true;
}

bool blob_socket_t::pending(int event) {
    unsigned long events;
    size_t size = sizeof(events);

    getsockopt(ZMQ_EVENTS, &events, &size);
    return events & event;
}

bool blob_socket_t::has_more() {
    int64_t rcvmore;
    size_t size = sizeof(rcvmore);

    getsockopt(ZMQ_RCVMORE, &rcvmore, &size);
    return rcvmore != 0;
}

int blob_socket_t::fd() {
    int fd;
    size_t size = sizeof(fd);

    getsockopt(ZMQ_FD, &fd, &size);
    return fd;
}

