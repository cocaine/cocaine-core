#include <stdexcept>

#include "engines.hpp"

using namespace yappi::engines;
using namespace yappi::plugins;

loop_t::loop_t(const std::string& key, source_t* source, zmq::context_t& context, time_t interval, time_t ttl): 
    m_refs(1),
    m_ttl(ttl),
    m_workload(key, source, context, interval)
{
    syslog(LOG_DEBUG, "starting engine %s", key.c_str());
    
    // Remeber the time when we started and fire off the thread
    clock_gettime(CLOCK_REALTIME, &m_timestamp);
    int result = pthread_create(&m_thread, NULL, &poll, &m_workload);
    
    if(result == EAGAIN) {
        throw std::runtime_error("thread limit exceeded");
    }
}

loop_t::~loop_t() {
    syslog(LOG_DEBUG, "stopping engine %s", m_workload.key.c_str());

    // Signal the termination
    pthread_cond_signal(&m_workload.terminate);
    pthread_join(m_thread, NULL);
    
    // Deallocate the source object, which was allocated by the plugin registry
    delete m_workload.source;

    // Reset reference counter, so the engine could be reaped
    m_refs = 0;
}

void loop_t::subscribe(time_t interval, time_t ttl) {
    syslog(LOG_DEBUG, "updating engine %s with interval: %lu, ttl: %lu",
        m_workload.key.c_str(), interval, ttl);

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
    pthread_spin_lock(&m_workload.datalock);
    clock_parse(interval, m_workload.interval);
    pthread_spin_unlock(&m_workload.datalock);
}

bool loop_t::reapable(const timespec& now) {
    return ((m_ttl && (now.tv_sec > m_timestamp.tv_sec + m_ttl)) ||
       !m_refs);
}

void* loop_t::poll(void* arg) {
    workload_t* workload = reinterpret_cast<workload_t*>(arg);

    // Connecting
    zmq::socket_t socket(workload->context, ZMQ_PUSH);
    socket.connect("inproc://events");

    // Preparing
    event_t event(workload->key);
    timespec timer;

    while(true) {
        clock_gettime(CLOCK_REALTIME, &timer);
        
        // Fetch new data
        // The core is responsible for delete-ing this dict
        event.dict = new dict_t(workload->source->fetch());

        // Sending
        zmq::message_t message(sizeof(event));
        memcpy(message.data(), &event, sizeof(event));
        socket.send(message);

        // Preparing to sleep
        pthread_spin_lock(&workload->datalock);
        clock_advance(timer, workload->interval);
        pthread_spin_unlock(&workload->datalock);
        
        // Using a timed wait to sleep here, so the engine could be able
        // to signal the termination conditition
        if(pthread_cond_timedwait(&workload->terminate, &workload->sleeplock, &timer) == 0) {
            return NULL;
        }
    }
}
