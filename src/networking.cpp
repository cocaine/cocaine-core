#include "cocaine/networking.hpp"

using namespace cocaine::lines;

bool socket_t::pending(int event) {
    unsigned long events;
    size_t size = sizeof(events);

    getsockopt(ZMQ_EVENTS, &events, &size);

    return events & event;
}

bool socket_t::has_more() {
    int64_t rcvmore;
    size_t size = sizeof(rcvmore);

    getsockopt(ZMQ_RCVMORE, &rcvmore, &size);

    return rcvmore != 0;
}

int socket_t::fd() {
    int fd;
    size_t size = sizeof(fd);

    getsockopt(ZMQ_FD, &fd, &size);

    return fd;
}

namespace msgpack {
    template<> Json::Value& operator>>(msgpack::object o, Json::Value& v) {
	    if(o.type != type::RAW) { 
            throw type_error();
        }
	
        std::string json(o.via.raw.ptr, o.via.raw.size);
        Json::Reader reader(Json::Features::strictMode());
        v.clear();

        if(!reader.parse(json, v)) {
            throw std::runtime_error("corrupted json in the channel");
        }

        return v;
    }
}

