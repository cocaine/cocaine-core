#include <stdexcept>
#include <sstream>

#include "engine.hpp"

using namespace yappi::engine;
using namespace yappi::plugin;

engine_t::engine_t(const std::string& uri, source_t* source, zmq::context_t& context):
    m_uri(uri),
    m_socket(context, ZMQ_PAIR)
{
    // Bind the controlling socket
    m_socket.bind(("inproc://" + m_uri).c_str());
    
    // Create a new task object for the thread
    task_t* task = new task_t(m_uri, source, context);

    // And start the thread
    if(pthread_create(&m_thread, NULL, &bootstrap, task) == EAGAIN) {
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

std::string engine_t::schedule(const std::deque<std::string>& identity, time_t interval) {
    // Generate the subscription key
    std::ostringstream fmt;
    fmt << m_uri << interval;
    std::string key = m_digest.get(fmt.str());

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

void engine_t::deschedule(const std::deque<std::string>& identity, time_t interval) {
    // Generate the subscription key
    std::ostringstream fmt;
    fmt << m_uri << interval;
    std::string key = m_digest.get(fmt.str());

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

    throw std::invalid_argument("client is not a subscriber");
}

void* engine_t::bootstrap(void* arg) {
    // Unpack the task
    task_t* task = reinterpret_cast<task_t*>(arg);

    // Start the overseer. This blocks until stopped manually
    overseer_t overseer(*task);
    overseer.run();

    // Cleanup
    delete task->source;
    delete task;

    return NULL;
}

engine_t::overseer_t::overseer_t(task_t& task):
    m_loop(),
    m_io(m_loop),
    m_task(task),
    m_socket(m_task.context, ZMQ_PAIR)
{
    syslog(LOG_DEBUG, "starting %s overseer", m_task.uri.c_str());
    
    // Damn you, 0MQ
    m_loop.set_io_collect_interval(0.5);

    // Set the socket watcher
    int fd;
    size_t size = sizeof(fd);

    m_socket.getsockopt(ZMQ_FD, &fd, &size);
    m_io.set(this);
    m_io.start(fd, EV_READ | EV_WRITE);

    // Connect to the engine's controlling socket
    m_socket.connect(("inproc://" + m_task.uri).c_str());
}

void engine_t::overseer_t::run() {
    m_loop.loop();
}

void engine_t::overseer_t::operator()(ev::io& io, int revents) {
    unsigned long events;
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
            reinterpret_cast<char*>(message.data()),
            message.size());

        if(cmd == "schedule") {
            // Receive the key
            m_socket.recv(&message);
            std::string key(
                reinterpret_cast<char*>(message.data()),
                message.size());
 
            // Receive the interval
            time_t interval;

            m_socket.recv(&message);
            memcpy(&interval, message.data(), message.size());

            // Fire off a new slave
            syslog(LOG_DEBUG, "starting %s slave %s with interval: %lums",
                m_task.uri.c_str(), key.c_str(), interval);
            slave_t* slave = new slave_t(m_loop, m_task, key, interval);
    
            // And store it into the slave map
            m_slaves[key] = slave;
        } else if(cmd == "deschedule") {
            // Receive the key
            m_socket.recv(&message);
            std::string key(
                reinterpret_cast<char*>(message.data()),
                message.size());  

            // Kill the slave
            syslog(LOG_DEBUG, "stopping %s slave %s",
                m_task.uri.c_str(), key.c_str());
        
            slave_map_t::iterator it = m_slaves.find(key);
            
            delete it->second;
            m_slaves.erase(it);
        } else if(cmd == "stop") {
            syslog(LOG_DEBUG, "stopping %s overseer", m_task.uri.c_str());

            // Kill all the slaves
            for(slave_map_t::iterator it = m_slaves.begin(); it != m_slaves.end(); ++it) {
                delete it->second;
            }

            // After this, the event loop should unroll
            m_io.stop();
        }
    }
}

engine_t::slave_t::slave_t(ev::dynamic_loop& loop, task_t& task, const std::string& key, time_t interval):
    m_timer(loop),
    m_source(task.source),
    m_socket(task.context, ZMQ_PUSH),
    m_key(key)
{
    // Connect to the core
    m_socket.connect("inproc://sink");
   
    // Start up the timer 
    m_timer.set(this);
    m_timer.start(interval / 1000.0, interval / 1000.0);
}

engine_t::slave_t::~slave_t() {
    m_timer.stop();
}

void engine_t::slave_t::operator()(ev::timer& timer, int revents) {
    dict_t* dict;

    try {
        dict = new dict_t(m_source->fetch());
        
        // Do nothing if plugin has returned an empty dict
        if(dict->size() == 0) {
            delete dict;
            return;
        }
    } catch(const std::exception& e) {
        delete dict;
        dict = new dict_t();

        dict->insert(std::make_pair("exception", e.what()));
    }   

    zmq::message_t message(m_key.length());
    memcpy(message.data(), m_key.data(), m_key.length());
    m_socket.send(message, ZMQ_SNDMORE);

    message.rebuild(sizeof(dict));
    memcpy(message.data(), &dict, sizeof(dict));
    m_socket.send(message);
}
