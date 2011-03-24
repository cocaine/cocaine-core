#include "core.hpp"

timed_core_t::timed_core_t(char* ep_req, char* ep_export, int64_t watermark, unsigned int io_threads, time_t interval):
    core_t(ep_req, ep_export, watermark, io_threads)
{
    syslog(LOG_INFO, "running on timed core, interval: %lums", interval);
    clock_parse(interval, m_interval);
}

void timed_core_t::start() {
    zmq::message_t message;
    m_running = true;

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
    //     4.2. If there is, dissassemle it into fields and append them to pending list
    //  6. Loop until there are no events left in the queue, or watermark is hit
    //  7. Publish everything from the pending list
    //  8. Sleep until stored time + interval

    while(m_running) {
        // 1. Store the current time
        clock_gettime(CLOCK_REALTIME, &m_now);

        for(int32_t cnt = 0;; ++cnt) {
            // 2. Try to fetch the next request
            if(!s_requests.recv(&message, ZMQ_NOBLOCK)) {
                if(cnt)
                    syslog(LOG_DEBUG, "dispatched %d requests", cnt);
                break;
            }

            // 2.*. Dispatch the request
            std::string request(
                reinterpret_cast<char*>(message.data()),
                message.size());
            dispatch(request);
        }

        // 4. Reap all reapable engines
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

        for(int32_t cnt = 0; cnt < m_watermark; ++cnt) {
            // 5. Try to fetch the next event
            if(!s_events.recv(&message, ZMQ_NOBLOCK)) {
                if(cnt)
                    syslog(LOG_DEBUG, "processed %d events", cnt);
                break;
            }

            // 5.*. Process the event
            event_t* event = reinterpret_cast<event_t*>(message.data());
            feed(*event);
        }

        // 7. Publish
        if(m_pending.size())
            syslog(LOG_DEBUG, "publishing %d items", m_pending.size());
        
        for(pending_t::iterator it = m_pending.begin(); it != m_pending.end(); ++it) {
            message.rebuild(it->length());
            memcpy(message.data(), it->data(), it->length());
            s_export.send(message);
        }

        m_pending.clear();

        // 8. Sleeping
        clock_advance(m_now, m_interval);
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &m_now, NULL);
    }
}

void timed_core_t::stop() {
    m_running = false;
}

void timed_core_t::feed(event_t& event) {
    if(m_engines.find(event.key) == m_engines.end()) {
        // 5.1. Outstanding event from a stopped engine
        syslog(LOG_DEBUG, "discarding event for %s", event.key.c_str());
        delete event.dict;
        return;
    }

    // 6. Disassemble
    for(dict_t::iterator it = event.dict->begin(); it != event.dict->end(); ++it) {
        std::ostringstream fmt;
        fmt << event.key << " " << it->first << "=" << it->second << " @" << m_now.tv_sec;
        m_pending.push_back(fmt.str());
    }

    delete event.dict;
}

