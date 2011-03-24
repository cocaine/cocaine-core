#include <signal.h>

#include "core.hpp"

poll_core_t::poll_core_t(char* ep_req, char* ep_export, int64_t watermark, unsigned int io_threads, time_t interval):
    core_t(ep_req, ep_export, watermark, io_threads),
    m_interval(interval * 1000)
{
    syslog(LOG_INFO, "running on a polling core, interval: %lums", interval);
}

void poll_core_t::start() {
    zmq_pollitem_t sockets[2];
    sockets[0].socket = (void*)s_requests;
    sockets[0].events = ZMQ_POLLIN;
    sockets[1].socket = (void*)s_events;
    sockets[1].events = ZMQ_POLLIN;

    zmq::message_t message;

    // Loop structure:
    // ---------------
    //  1. Store the current time
    //  2. Fetch next request
    //     2.1. If it's a subscribtion:
    //          2.1.1. Generate the key
    //          2.1.2. Check if there is an active engine for the key
    //          2.1.3. If there is, adjust interval and ttl with max(old, new)
    //                 and increment reference counter by one
    //          2.1.4. If there is not, launch a new one
    //          2.1.5. Return the key
    //     2.2. If it's an unsubscription:
    //          2.2.1. Check if there is an active engine for the key
    //          2.2.2. If there is, decrement its reference counter by one
    //  3. Loop until there are no requests left in the queue
    //  4. Reap all reapable engines
    //  5. Fetch next event
    //     4.1. If there no engines with the specified id, discard it
    //     4.2. If there is, dissassemle it into fields and publish
    //  6. Loop until there are no events left in the queue
    //  7. Sleep until something happends

    while(true) {
        // 1. Store the current time
        clock_gettime(CLOCK_REALTIME, &m_now);

        // 2. Fetching requests, if any
        if(sockets[0].revents == ZMQ_POLLIN) {
            for(int32_t cnt = 0;; ++cnt) {
                if(!s_requests.recv(&message, ZMQ_NOBLOCK)) {
                    syslog(LOG_DEBUG, "dispatched %d requests", cnt);
                    break;
                }

                // 2.*. Dispatch the request
                std::string request(
                    reinterpret_cast<char*>(message.data()),
                    message.size());
                dispatch(request);
            }
        }

        // 4. Reap all reapable engines
        // This kills those engines whose ttl has expired
        // or whose reference counter dropped to zero
        engines_t::iterator it = m_engines.begin();
        while(it != m_engines.end()) {
            if(it->second->reapable(m_now)) {
                syslog(LOG_DEBUG, "reaping engine %s", it->first.c_str());
                delete it->second;
                m_engines.erase(it++);
            } else {
                ++it;
            }
        }

        // 5. Fetching new events, if any
        if(sockets[1].revents == ZMQ_POLLIN) {
            for(int32_t cnt = 0;; ++cnt) {
                if(!s_events.recv(&message, ZMQ_NOBLOCK)) {
                    syslog(LOG_DEBUG, "processed %d events", cnt);
                    break;
                }

                // 5.*. Publish the event
                event_t *event = reinterpret_cast<event_t*>(message.data());
                publish(*event);
            }
        }

        // 8. Sleeping
        // TODO: Make it absolute again
        try {
            zmq::poll(sockets, 2, m_interval);
        } catch(const zmq::error_t& e) {
            if(e.num() == EINTR && (m_signal == SIGINT || m_signal == SIGTERM)) {
                return;
            } else {
                syslog(LOG_ERR, "something bad happened: %s", e.what());
                return;
            }
        }
    }
}

void poll_core_t::publish(event_t& event) {
    zmq::message_t message;
        
    if(m_engines.find(event.key) == m_engines.end()) {
        // 5.1. Outstanding event from a stopped engine
        syslog(LOG_DEBUG, "discarding event for %s", event.key.c_str());
        delete event.dict;
        return;
    }

    // 6. Disassemble and publish
    for(dict_t::iterator it = event.dict->begin(); it != event.dict->end(); ++it) {
        std::ostringstream fmt;
        fmt << event.key << " " << it->first << "=" << it->second << " @" << m_now.tv_sec;
    
        message.rebuild(fmt.str().length());
        memcpy(message.data(), fmt.str().data(), fmt.str().length());
        s_export.send(message);    
    }

    delete event.dict;
}

