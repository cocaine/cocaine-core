#include "cocaine/networking.hpp"

using namespace cocaine::net;

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

bool json_socket_t::send_json(const Json::Value& root, int flags) {
    return send_object(Json::FastWriter().write(root), flags);
}

bool json_socket_t::recv_json(Json::Value& root, int flags) {
    std::string request;
    Json::Reader reader(Json::Features::strictMode());

    if(!recv_object(request, flags)) {
        return false;
    }

    if(!reader.parse(request, root)) {
        syslog(LOG_ERR, "net: invalid json - %s",
            reader.getFormatedErrorMessages().c_str());
        return false;
    }

    return true;
}

