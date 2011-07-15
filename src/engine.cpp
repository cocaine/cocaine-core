#include <stdexcept>
#include <sstream>
#include <functional>

#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <msgpack.hpp>

#include "engine.hpp"
#include "future.hpp"

using namespace yappi::core;
using namespace yappi::engine;
using namespace yappi::plugin;

engine_t::engine_t(zmq::context_t& context, source_t* source):
    m_id(),
    m_thread(NULL),
    m_source(source),
    m_hash(helpers::digest_t().get(source->uri())),
    m_context(context),
    m_pipe(m_context, ZMQ_PUSH)
{
    // Bind the controlling socket and fire of the thread
    m_pipe.bind("inproc://" + m_id.get());
    m_thread = new boost::thread(boost::bind(&engine_t::bootstrap, this));
}

engine_t::~engine_t() {
    Json::Value message;

    message["command"] = "stop";
    m_pipe.send(message);

    // Wait for it to stop
    m_thread->join();
    delete m_thread;

    // Get rid of the source
    delete m_source;
}

void engine_t::push(const future_t* future, time_t interval) {
    Json::Value message;

    message["command"] = "push";
    message["future"] = future->id();
    message["token"] = future->token();
    message["interval"] = Json::UInt(interval);
    
    m_pipe.send(message);
}

void engine_t::drop(const future_t* future, time_t interval) {
    Json::Value message;

    message["command"] = "drop";
    message["future"] = future->id();
    message["token"] = future->token();
    message["interval"] = Json::UInt(interval);
    
    m_pipe.send(message);
}

void engine_t::once(const future_t* future) {
    Json::Value message;

    message["command"] = "once";
    message["future"] = future->id();

    m_pipe.send(message);
}

void engine_t::bootstrap() {
    // This blocks until stopped manually
    overseer_t overseer(m_context, m_source, m_hash, m_id);
    overseer.run();
}

overseer_t::overseer_t(zmq::context_t& context, source_t* source, const std::string& hash, const helpers::id_t& id):
    m_loop(),
    m_io(m_loop),
    m_stall(m_loop),
    m_source(source),
    m_hash(hash),
    m_context(context),
    m_pipe(m_context, ZMQ_PULL),
    m_futures(m_context, ZMQ_PUSH),
    m_reaper(m_context, ZMQ_PUSH),
    m_storage("/var/spool/yappi")
{
    syslog(LOG_DEBUG, "%s overseer: starting", m_source->uri().c_str());
    
    // Connect to the engine's controlling socket
    // and set the socket watcher
    int fd;
    size_t size = sizeof(fd);

    m_pipe.connect("inproc://" + id.get());
    m_pipe.getsockopt(ZMQ_FD, &fd, &size);
    m_io.set(this);
    m_io.start(fd, EV_READ);

    // Initializing stall timer
    m_stall.set(this);
    m_stall.start(60.);

    // Connecting to the core's future sink
    m_futures.connect("inproc://futures");

    // Connecting to the core's reaper sink
    m_reaper.connect("inproc://reaper");

    // Signal a false event, in case engine has managed to send something already
    m_loop.feed_fd_event(fd, EV_READ);
}

void overseer_t::run() {
    m_loop.loop();
}

void overseer_t::operator()(ev::io& io, int revents) {
    std::string command;
    
    while(m_pipe.pending()) {
        Json::Value message;
        
        m_pipe.recv(message);
        command = message["command"].asString();

        if(command == "push") {
            push(message);
        } else if(command == "drop") {
            drop(message);
        } else if(command == "once") {
            once(message);
        } else if(command == "stop") {
            stop(message);
        }
    }
}
 
void overseer_t::operator()(ev::timer& timer, int revents) {
    Json::Value message;

    message["engine"] = m_source->uri();

    // This is a suicide ;(
    m_reaper.send(message);    
}

void overseer_t::push(const Json::Value& message) {
    Json::Value response, result;
    
    // Unpack
    time_t interval = message["interval"].asUInt();
    std::string token = message["token"].asString();

    // Generate the subscription key
    std::ostringstream key;
    key << m_hash << ":" << interval;

    // If there's no slave for this interval yet, start a new one
    if(m_slaves.find(interval) == m_slaves.end()) {
        syslog(LOG_DEBUG, "%s overseer: scheduling for execution every %lu ms",
            m_source->uri().c_str(), interval);
    
        ev::timer* slave = new ev::timer(m_loop);
        slave->set(new fetcher_t(m_context, m_source, key.str()));
        slave->start(interval / 1000.0, interval / 1000.0);
        m_slaves.insert(interval, slave);

        // Stop the stall timer, if it was running
        if(m_stall.is_active()) {
            syslog(LOG_DEBUG, "%s overseer: stall timer stopped", m_source->uri().c_str());
            m_stall.stop();
        }
    }

    // Save token to control unsubscription access rights, if it's not there already
    subscription_map_t::iterator begin, end;
    subscription_map_t::value_type subscription = std::make_pair(interval, token);
    boost::tie(begin, end) = m_subscriptions.equal_range(interval);
    std::equal_to<subscription_map_t::value_type> predicate;
    
    if(std::find_if(begin, end, boost::bind(predicate, subscription, _1)) == end) {
        m_subscriptions.insert(subscription);
    }

    // Persist
    std::string object_id = m_digest.get(key.str() + token);
    Json::Value object;

    object["url"] = m_source->uri();
    object["interval"] = static_cast<int32_t>(interval);
    object["token"] = token;
    m_storage.put(object_id, object); 

    // Report to the core
    result["key"] = key.str();
    response["future"] = message["future"];
    response["engine"] = m_source->uri();
    response["result"] = result;
    m_futures.send(response);
}

void overseer_t::drop(const Json::Value& message) {
    Json::Value response, result;
    
    // Unpack
    time_t interval = message["interval"].asUInt();
    std::string token = message["token"].asString();

    // Check if we have such a slave
    slave_map_t::iterator slave = m_slaves.find(interval);

    if(slave == m_slaves.end()) {
        result["error"] = "not found";
    } else {
        // Check if the client is eligible for unsubscription
        subscription_map_t::iterator begin, end, subscriber;
        subscription_map_t::value_type subscription = std::make_pair(interval, token);
        boost::tie(begin, end) = m_subscriptions.equal_range(interval);
        std::equal_to<subscription_map_t::value_type> predicate;
        
        subscriber = std::find_if(begin, end, boost::bind(predicate, subscription, _1));

        if(subscriber == end) {
            result["error"] = "not authorized";
        } else {
            syslog(LOG_DEBUG, "%s overseer: descheduling from execution every %lums",
                m_source->uri().c_str(), interval);
            
            // Unsubscribe
            slave->second->stop();
            delete static_cast<fetcher_t*>(slave->second->data);
            m_slaves.erase(slave);
            m_subscriptions.erase(subscriber);
            
            // Remove the task from the storage
            std::ostringstream key;
            key << m_hash << ":" << interval;

            std::string object_id = m_digest.get(key.str() + token);
            m_storage.remove(object_id);
            
            // Start the stall timer if this was the last slave
            if(!m_slaves.size()) {
                syslog(LOG_DEBUG, "%s overseer: stall timer started", m_source->uri().c_str());
                m_stall.start(60.);
            }

            result["status"] = "success";
        }
    }

    // Report to the core
    response["future"] = message["future"];
    response["engine"] = m_source->uri();
    response["result"] = result;
    m_futures.send(response);
}

void overseer_t::once(const Json::Value& message) {
    Json::Value response, result;
    
    dict_t dict;

    try {
        dict = m_source->fetch();
    } catch(const std::exception& e) {
        syslog(LOG_ERR, "plugin %s invocation failed: %s",
            m_source->uri().c_str(), e.what());
        result["error"] = "invocation failed";
    }
        
    if(!result.isMember("error")) {
        for(dict_t::const_iterator it = dict.begin(); it != dict.end(); ++it) {
            result[it->first] = it->second;
        }
    }

    response["future"] = message["future"];
    response["engine"] = m_source->uri();
    response["result"] = result;
    m_futures.send(response);

    // Rearm the stall timer if it's active
    if(m_stall.is_active()) {
        syslog(LOG_DEBUG, "%s overseer: stall timer rearmed", m_source->uri().c_str());
        m_stall.stop();
        m_stall.start(60.);
    }
}

void overseer_t::stop(const Json::Value& message) {
    syslog(LOG_DEBUG, "%s overseer: stopping", m_source->uri().c_str());

    // Kill all the slaves
    for(slave_map_t::iterator it = m_slaves.begin(); it != m_slaves.end(); ++it) {
        it->second->stop();
        delete static_cast<fetcher_t*>(it->second->data);
    }

    // Delete all the watchers and unroll the event loop
    m_slaves.clear();
    m_stall.stop();
    m_io.stop();
}

fetcher_t::fetcher_t(zmq::context_t& context, source_t* source, const std::string& key):
    m_source(source),
    m_uplink(context, ZMQ_PUSH),
    m_key(key)
{
    // Connect to the core
    m_uplink.connect("inproc://events");
}

void fetcher_t::operator()(ev::timer& timer, int revents) {
    dict_t dict;

    try {
        dict = m_source->fetch();
    } catch(const std::exception& e) {
        syslog(LOG_ERR, "plugin %s invocation failed: %s",
            m_source->uri().c_str(), e.what());
        return;
    }
        
    // Do nothing if plugin has returned an empty dict
    if(dict.size() == 0) {
        return;
    }

    zmq::message_t message(m_key.length());
    memcpy(message.data(), m_key.data(), m_key.length());
    m_uplink.send(message, ZMQ_SNDMORE);

    // Serialize the dict
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, dict);

    // And send it
    message.rebuild(buffer.size());
    memcpy(message.data(), buffer.data(), buffer.size());
    m_uplink.send(message);
}
