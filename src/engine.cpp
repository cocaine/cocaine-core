#include <stdexcept>
#include <sstream>

#include "engine.hpp"

using namespace yappi::engine;
using namespace yappi::plugin;

engine_t::engine_t(const std::string& id, source_t& source, zmq::context_t& context):
    m_id(id),
    m_socket(context, ZMQ_PAIR)
{
    // Bind the controlling socket
    m_socket.bind(("inproc://" + m_id).c_str());
    
    // Create a new task object for the thread
    // It is created on the heap so we could be able to detach
    // and forget about the thread (which, in turn, will deallocate
    // these resources at some point of time)
    task_t* task = new task_t(m_id, source, context);

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
    
    // Detach and forget, it will die eventually
    pthread_detach(m_thread);
}

std::string engine_t::subscribe(time_t interval) {
    // Generating the subscription key
    std::ostringstream fmt;
    fmt << m_id << ':' << interval;
    std::string key = m_digest.get(fmt.str());

    // Sending the data to the thread
    std::string cmd = "subscribe";
    zmq::message_t message(cmd.length());
    memcpy(message.data(), cmd.data(), cmd.length());
    m_socket.send(message, ZMQ_SNDMORE);

    message.rebuild(key.length());
    memcpy(message.data(), key.data(), key.length());
    m_socket.send(message, ZMQ_SNDMORE);

    message.rebuild(sizeof(interval));
    memcpy(message.data(), &interval, sizeof(interval));
    m_socket.send(message);

    m_keys.insert(key);
    return key;
}

bool engine_t::unsubscribe(const std::string& key) {
    std::set<std::string>::iterator it = m_keys.find(key);
    
    if(it == m_keys.end()) {
        return false;    
    }

    std::string cmd = "unsubscribe";
    zmq::message_t message(cmd.length());
    memcpy(message.data(), cmd.data(), cmd.length());
    m_socket.send(message, ZMQ_SNDMORE);

    message.rebuild(key.length());
    memcpy(message.data(), key.data(), key.length());
    m_socket.send(message);

    m_keys.erase(key);
    return true;
}

void* engine_t::bootstrap(void* arg) {
    task_t* task = reinterpret_cast<task_t*>(arg);

    overseer_t overseer(*task);
    overseer.run();

    // The thread is detached at this point
    delete &task->source;
    delete task;

    return NULL;
}

engine_t::overseer_t::overseer_t(task_t& task):
    m_loop(),
    m_io(m_loop),
    m_task(task),
    m_socket(m_task.context, ZMQ_PAIR)
{
    int fd;
    size_t size;

    syslog(LOG_DEBUG, "starting the overseer for: %s", m_task.id.c_str());
    
    // Integrating 0MQ into libev event loop
    m_socket.getsockopt(ZMQ_FD, &fd, &size);
    m_io.set(this);
    m_io.start(fd, EV_READ);
    
    m_socket.connect(("inproc://" + m_task.id).c_str());
}

engine_t::overseer_t::~overseer_t() {
    syslog(LOG_DEBUG, "stopping the overseer for: %s", m_task.id.c_str());
}

void engine_t::overseer_t::run() {
    m_loop.loop();
}

void engine_t::overseer_t::operator()(ev::io& io, int revents) {
    unsigned long event;
    size_t size;

    // Check if we have a right event on the socket
    // According to the 0MQ manual, the situation when the fd is ready
    // but we have no events on the 0MQ socket is valid
    m_socket.getsockopt(ZMQ_EVENTS, &event, &size);

    if(!(event & ZMQ_POLLIN)) {
        return;
    }
    
    zmq::message_t message;
    
    // Receive the actual message
    m_socket.recv(&message);

    std::string cmd(
        reinterpret_cast<char*>(message.data()),
        message.size()
    );

    if(cmd == "subscribe") {
        // Receiving the key
        m_socket.recv(&message);
   
        std::string key(
            reinterpret_cast<char*>(message.data()),
            message.size());
 
        // Receiving the interval
        m_socket.recv(&message);

        time_t interval = reinterpret_cast<time_t>(message.data());

        // Setting up a new slave
        syslog(LOG_DEBUG, "starting a slave for: %s, key: %s, interval: %lu",
            m_task.id.c_str(), key.c_str(), interval);
        slave_t* slave = new slave_t(m_loop, m_task, key, interval);
    
        // Storing the new slave
        m_slaves[key] = slave;
    
    } else if(cmd == "unsubscribe") {
        // Receiving the key
        m_socket.recv(&message);

        std::string key(
            reinterpret_cast<char*>(message.data()),
            message.size()
        );  

        // Killing the slave
        std::map<std::string, slave_t*>::iterator it = m_slaves.find(key);
        
        syslog(LOG_DEBUG, "stopping a slave for: %s, key: %s",
            m_task.id.c_str(), key.c_str());
        
        delete it->second;
        m_slaves.erase(it);

    } else if(cmd == "stop") {
        // Killing all the slaves
        for(std::map<std::string, slave_t*>::iterator it = m_slaves.begin();
            it != m_slaves.end(); ++it) {
                delete it->second;
        }

        m_slaves.clear();

        // After this, the event loop should unroll
        m_io.stop();
    }
}

engine_t::slave_t::slave_t(ev::dynamic_loop& loop, task_t& task, const std::string& key, time_t interval):
    m_timer(loop),
    m_source(task.source),
    m_socket(task.context, ZMQ_PUSH),
    m_key(key)
{
    m_socket.connect("inproc://events");
    
    m_timer.set(this);
    m_timer.start(interval / 1000.0);
}

engine_t::slave_t::~slave_t() {
    m_timer.stop();
}

void engine_t::slave_t::operator()(ev::timer& timer, int revents) {
    source_t::dict_t* dict;

    syslog(LOG_DEBUG, "slave iteration for key: %s", m_key.c_str());

    try {
        dict = new source_t::dict_t(m_source.fetch());
    } catch(const std::exception& e) {
        dict = new source_t::dict_t();
        dict->insert(std::make_pair("exception", e.what()));
    }   

    zmq::message_t message(m_key.length());
    memcpy(message.data(), m_key.data(), m_key.length());
    m_socket.send(message, ZMQ_SNDMORE);

    message.rebuild(sizeof(dict));
    memcpy(message.data(), dict, sizeof(dict));
    m_socket.send(message);

    m_timer.again();
}
