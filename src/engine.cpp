#include <stdexcept>
#include <sstream>

#include <msgpack.hpp>

#include "engine.hpp"
#include "digest.hpp"

using namespace yappi::engine;
using namespace yappi::plugin;

engine_t::engine_t(source_t* source, zmq::context_t& context):
    m_hash(helpers::digest_t().get(source->uri())),
    m_socket(context, ZMQ_PAIR)
{
    // Bind the controlling socket
    m_socket.bind(("inproc://" + m_hash).c_str());
    
    // Create a new task object for the thread
    task_t* task = new task_t(source, context);

    // And start the thread
    if(pthread_create(&m_thread, NULL, &bootstrap, task) == EAGAIN) {
        delete task;
        throw std::runtime_error("system thread limit exceeded");
    }
}

engine_t::~engine_t() {
    std::string cmd = "stop";
    zmq::message_t message(cmd.length());
    memcpy(message.data(), cmd.data(), cmd.length());
    m_socket.send(message);
    
    // Wait for it to stop
    pthread_join(m_thread, NULL);
}

std::string engine_t::schedule(const identity_t& identity, time_t interval) {
    // Generate the subscription key
    std::ostringstream fmt;
    fmt << m_hash << ':' << interval;
    std::string key = fmt.str();

    if(m_subscriptions.count(key) == 0) {
        // Slave is not running yet, start it
        std::string cmd = "schedule";
        zmq::message_t message(cmd.length());
        memcpy(message.data(), cmd.data(), cmd.length());
        m_socket.send(message, ZMQ_SNDMORE);

        message.rebuild(key.length());
        memcpy(message.data(), key.data(), key.length());
        m_socket.send(message, ZMQ_SNDMORE);

        message.rebuild(sizeof(interval));
        memcpy(message.data(), &interval, sizeof(interval));
        m_socket.send(message);
    }

    // Do some housekeeping
    m_subscriptions.insert(std::make_pair(key, identity.back()));

    // Return the subscription key
    return key;
}

void engine_t::deschedule(const identity_t& identity, const std::string& key) {
    // Unsubscribe the client if it is a subscriber
    std::pair<subscription_map_t::iterator, subscription_map_t::iterator> bounds =
        m_subscriptions.equal_range(key);

    for(subscription_map_t::iterator it = bounds.first; it != bounds.second; ++it) {
        if(it->second == identity.back()) {
            m_subscriptions.erase(it);

            // If it was the last subscriber, stop the slave
            if(m_subscriptions.count(key) == 0) {
                std::string cmd = "deschedule";
                zmq::message_t message(cmd.length());
                memcpy(message.data(), cmd.data(), cmd.length());
                m_socket.send(message, ZMQ_SNDMORE);

                message.rebuild(key.length());
                memcpy(message.data(), key.data(), key.length());
                m_socket.send(message);
            }

            return;
        } 
    }

    throw std::runtime_error("client is not a subscriber");
}

void* engine_t::bootstrap(void* arg) {
    // Unpack the task
    std::auto_ptr<task_t> task(reinterpret_cast<task_t*>(arg));

    // Start the overseer. This blocks until stopped manually
    overseer_t overseer(*task);
    overseer.run();

    return NULL;
}

engine_t::overseer_t::overseer_t(task_t& task):
    m_loop(),
    m_io(m_loop),
    m_task(task),
    m_socket(m_task.context, ZMQ_PAIR)
{
    syslog(LOG_DEBUG, "starting %s overseer", m_task.source->uri().c_str());
    
    // Set the socket watcher
    int fd;
    size_t size = sizeof(fd);

    // Connect to the engine's controlling socket
    m_socket.connect(("inproc://" + helpers::digest_t().get(
        m_task.source->uri())).c_str());

    m_socket.getsockopt(ZMQ_FD, &fd, &size);
    m_io.set(this);
    m_io.start(fd, EV_READ | EV_WRITE);

    m_loop.set_io_collect_interval(0.5);
}

void engine_t::overseer_t::run() {
    m_loop.loop();
}

void engine_t::overseer_t::operator()(ev::io& io, int revents) {
    uint32_t events;
    size_t size = sizeof(events);
    
    zmq::message_t message;
    std::string cmd;

    while(true) {
        // Check if we actually have something in the socket
        m_socket.getsockopt(ZMQ_EVENTS, &events, &size);

        if(!(events & ZMQ_POLLIN)) {
            break;
        }

        // And if we do, receive it 
        m_socket.recv(&message);
        cmd.assign(
            reinterpret_cast<const char*>(message.data()),
            message.size());

        if(cmd == "schedule") {
            // Receive the key
            m_socket.recv(&message);
            std::string key(
                reinterpret_cast<const char*>(message.data()),
                message.size());
 
            // Receive the interval
            time_t interval;

            m_socket.recv(&message);
            memcpy(&interval, message.data(), message.size());

            // Start a new slave
            syslog(LOG_DEBUG, "starting %s slave %s with interval: %lums",
                m_task.source->uri().c_str(), key.c_str(), interval);
            
            ev::timer* slave = new ev::timer(m_loop);
            slave->set(new fetcher_t(m_task, key));
            slave->start(interval / 1000.0, interval / 1000.0);
            
            m_slaves.insert(key, slave);

        } else if(cmd == "deschedule") {
            // Receive the key
            m_socket.recv(&message);
            std::string key(
                reinterpret_cast<const char*>(message.data()),
                message.size());  

            // Kill the slave
            syslog(LOG_DEBUG, "stopping %s slave %s",
                m_task.source->uri().c_str(), key.c_str());
        
            slave_map_t::iterator it = m_slaves.find(key);
            
            it->second->stop();
            delete static_cast<fetcher_t*>(it->second->data);
            
            m_slaves.erase(it);

        } else if(cmd == "stop") {
            syslog(LOG_DEBUG, "stopping %s overseer", m_task.source->uri().c_str());

            // Kill all the slaves
            for(slave_map_t::iterator it = m_slaves.begin(); it != m_slaves.end(); ++it) {
                it->second->stop();
                delete static_cast<fetcher_t*>(it->second->data);
            }

            m_slaves.clear();

            // After this, the event loop should unroll
            m_io.stop();
        }
    }
}

engine_t::fetcher_t::fetcher_t(task_t& task, const std::string& key):
    m_key(key),
    m_task(task),
    m_socket(task.context, ZMQ_PUSH)
{
    // Connect to the core
    m_socket.connect("inproc://sink");
}

void engine_t::fetcher_t::operator()(ev::timer& timer, int revents) {
    dict_t dict;

    try {
        dict = m_task.source->fetch();
    } catch(const std::exception& e) {
        syslog(LOG_ERR, "plugin %s invocation failed: %s",
            m_task.source->uri().c_str(), e.what());
        return;
    }
        
    // Do nothing if plugin has returned an empty dict
    if(dict.size() == 0) {
        return;
    }

    zmq::message_t message(m_key.length());
    memcpy(message.data(), m_key.data(), m_key.length());
    m_socket.send(message, ZMQ_SNDMORE);

    // Serialize the dict
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, dict);

    message.rebuild(buffer.size());
    memcpy(message.data(), buffer.data(), buffer.size());
    m_socket.send(message);
}
