#include "client.hpp"

#include <boost/lambda/bind.hpp>
#include <boost/format.hpp>

#include <uuid/uuid.h>
#include "json/json.h"

#define UUID_STRING_SIZE 37

using namespace yappi::client;

consumer_base_t::consumer_base_t(client_t& parent):
    m_parent(parent)
{
    uuid_t uuid;
    char unparsed_uuid[UUID_STRING_SIZE];

    uuid_generate(uuid);
    uuid_unparse(uuid, unparsed_uuid);
    m_uuid = unparsed_uuid;
}

consumer_base_t::~consumer_base_t() {
    m_parent.unsubscribe(m_uuid);
}

bool consumer_base_t::subscribe(const std::vector<std::string>& urls, const std::string& field) {
    return m_parent.subscribe(urls, field, this);
}

bool consumer_base_t::subscribe(const std::string& url, const std::string& field) {
    std::vector<std::string> urls;
    urls.push_back(url);

    return m_parent.subscribe(urls, field, this);
}

client_t::client_t():
    m_context(1),
    m_control(m_context, ZMQ_REQ),
    m_sink(m_context, ZMQ_SUB),
    m_pipe(m_context, ZMQ_PAIR)
{
    int linger = 0;

    // Disable socket linger
    m_control.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
    m_sink.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
    
    // Setup the thread interconnection pipe
    m_pipe.bind("inproc://yappi-interthread-pipe");

    // Start the thread
    m_thread = new boost::thread(boost::lambda::bind(&client_t::processor, this));

    // XXX: Drop this
    connect("tcp://localhost:5000", "tcp://localhost:5001");
}

client_t::~client_t() {
    // Signal the thread
    char stop[] = "stop";
    zmq::message_t msg(stop, sizeof(stop), NULL);
    m_pipe.send(msg);

    // Wait for it to finish
    m_thread->join();
    delete m_thread;
}

void client_t::connect(const std::string& control, const std::string& sink) {
    m_control.connect(control.c_str());
    m_sink.connect(sink.c_str());
}

bool client_t::subscribe(const std::vector<std::string>& urls, const std::string& field, consumer_base_t* consumer) {
    boost::mutex::scoped_lock lock(m_mutex);
    Json::Value root;

    // Request JSON header
    root["action"] = "push";
    root["version"] = "1";

    // Preparing the request JSON
    for(std::vector<std::string>::const_iterator it = urls.begin(); it != urls.end(); ++it) {
        root["targets"][*it] = Json::Value();
        root["targets"][*it]["interval"] = 5000;
    }

    // Performing the request
    Json::FastWriter writer;
    std::string request = writer.write(root);

    zmq::message_t request_message(request.size());
    memcpy(request_message.data(), request.data(), request.size());
    m_control.send(request_message);

    // Awating for the response
    zmq::message_t response_message;
    m_control.recv(&response_message);

    std::string response(
        static_cast<char*>(response_message.data()),
        response_message.size());

    // Parse the response JSON
    Json::Value subscriptions;
    Json::Reader reader(Json::Features::strictMode());
    
    if(!reader.parse(response, subscriptions)) {
        return false;
    }

    Json::Value::Members sources = subscriptions.getMemberNames();
        
    // Configure the dispatchers
    for(Json::Value::Members::const_iterator source = sources.begin(); source != sources.end(); ++source) {
        std::string key = subscriptions[*source]["key"].asString();
       
        // Store the target-key relationship
        m_sources[key] = *source;
       
        // Setup the dispatcher
        m_dispatch.insert(std::make_pair(
            std::make_pair(key, field),
            consumer));
        
        // Enable the subscriber socket to receive the messages
        std::string prefix = (boost::format("%1% %2%") % key % field).str();
        m_sink.setsockopt(ZMQ_SUBSCRIBE, prefix.data(), prefix.size());
    }

    return true;
}

void client_t::unsubscribe(const std::string& uuid) {
    boost::mutex::scoped_lock lock(m_mutex);
    dispatch_map_t::iterator it = m_dispatch.begin();

    // Drop it from dispatch map
    while(it != m_dispatch.end()) {
        if(it->second->uuid() == uuid) {
            m_dispatch.erase(it++);
        } else {
            ++it;
        }
    }

    // Add unsubcription control message
    // ...
}

void client_t::processor() {
    zmq::message_t msg;
    zmq::socket_t pipe(m_context, ZMQ_PAIR);
    std::string key, field, payload, time;
    time_t timestamp;

    // Connect the controlling pipe
    pipe.connect("inproc://yappi-interthread-pipe");

    zmq_pollitem_t sockets[] = {
        { (void*)pipe, 0, ZMQ_POLLIN, 0 },
        { (void*)m_sink, 0, ZMQ_POLLIN, 0 }
    };

    while(true) {
        // Poll for events
        while(zmq_poll(sockets, 2, -1) < 0);

        if(sockets[0].revents & ZMQ_POLLIN) {
            // We got something on the control socket
            // As of now, it can only be the termination signal
            // So stop the loop
            break;
        }

        if(sockets[1].revents & ZMQ_POLLIN) {
            // Get the envelope
            m_sink.recv(&msg);
            std::string envelope(
                static_cast<char*>(msg.data()),
                msg.size());

            std::istringstream xtr(envelope);
            
            xtr >> std::skipws >> key >> field >> time;
            timestamp = atoi(time.substr(1, time.find_first_of(".")).c_str());

            // Get the payload
            m_sink.recv(&msg);
            payload.assign(
                static_cast<char*>(msg.data()),
                msg.size());

            {
                boost::mutex::scoped_lock lock(m_mutex);
                dispatch_map_t::iterator it, end;

                boost::tie(it, end) = m_dispatch.equal_range(std::make_pair(key, field));
                std::string source = m_sources[key];

                while(it != end) {
                    it->second->dispatch(source, payload, timestamp);
                    ++it;
                }
            }
        }
    }
}
