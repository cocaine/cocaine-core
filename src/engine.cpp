#include "core.hpp"

engine_t::engine_t(const std::string& uri, source_t* source, zmq::context_t& context, time_t interval, time_t ttl): 
    m_refs(1),
    m_ttl(ttl),
    m_workload(uri, source, context, interval)
{
    syslog(LOG_DEBUG, "starting engine %s", uri.c_str());
    
    // Remeber the time when we started, then start
    clock_gettime(CLOCK_REALTIME, &m_timestamp);
    int result = pthread_create(&m_thread, NULL, &poll, &m_workload);
    
    if(result == EAGAIN) {
        syslog(LOG_ERR, "thread limit exceeded");
        throw std::exception();
    }
}

engine_t::~engine_t() {
    syslog(LOG_DEBUG, "stopping engine %s", m_workload.uri.c_str());

    // Set the stop flag
    // And wait for it to stop
    m_workload.running = false;
    pthread_join(m_thread, NULL);

    // Reset reference counter, so the engine could be reaped
    m_refs = 0;
}

void engine_t::subscribe(time_t interval, time_t ttl) {
    syslog(LOG_DEBUG, "updating engine %s with interval: %lu, ttl: %lu",
        m_workload.uri.c_str(), interval, ttl);

    // Incrementing the reference counter
    m_refs++;

    // Updating TTL
    if(!ttl) {
        m_ttl = 0;
    } else {
        // Updating the timestamp, for TTL calculations
        clock_gettime(CLOCK_REALTIME, &m_timestamp);
        m_ttl = m_ttl > ttl ? m_ttl : ttl;
    }

    // Send updated interval to the thread
    // TODO: Check if its shorter than the previous
    pthread_spin_lock(&m_workload.lock);
    clock_parse(interval, m_workload.interval);
    pthread_spin_unlock(&m_workload.lock);
}

bool engine_t::reapable(const timespec& now) {
    return ((m_ttl && (now.tv_sec > m_timestamp.tv_sec + m_ttl)) ||
       !m_refs);
}

void* engine_t::poll(void* arg) {
    workload_t* workload = reinterpret_cast<workload_t*>(arg);

    // Connecting
    zmq::socket_t socket(workload->context, ZMQ_PUSH);
    socket.connect("inproc://events");

    // Preparing
    event_t event(workload->uri);
    timespec timer;

    while(workload->running) {
        clock_gettime(CLOCK_REALTIME, &timer);
        
        // Fetch new data
        // The core is responsible for delete-ing this dict
        event.dict = new dict_t(workload->source->fetch());

        // Sending
        zmq::message_t message(sizeof(event));
        memcpy(message.data(), &event, sizeof(event));
        socket.send(message);

        // Sleeping
        pthread_spin_lock(&workload->lock);
        clock_advance(timer, workload->interval);
        pthread_spin_unlock(&workload->lock);
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &timer, NULL);
    }
}
