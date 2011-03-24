#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include "core.hpp"
#include "registry.hpp"

const char core_t::identity[] = "yappi";
const int core_t::version[] = { 0, 0, 1 };

extern registry_t* theRegistry;

core_t::core_t(char* ep_req, char* ep_export, int64_t watermark, unsigned int io_threads):
    m_context(io_threads),
    s_requests(m_context, ZMQ_REP),
    s_events(m_context, ZMQ_PULL),
    s_export(m_context, ZMQ_PUB),
    m_signal(0)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);
    syslog(LOG_INFO, "using libzmq v%d.%d.%d",
        major, minor, patch);

    // Argument dump
    syslog(LOG_INFO, "watermark: %llu events, %d io threads",
        watermark, io_threads);

    // Socket for requests
    try {
        s_requests.bind(ep_req);
        syslog(LOG_INFO, "listening on %s", ep_req);
    } catch(const zmq::error_t& e) {
        syslog(LOG_EMERG, "cannot bind to %s: %s", ep_req, e.what());
        exit(EXIT_FAILURE);
    }
    
    // Socket for exporting data
    try {
        s_export.bind(ep_export);
        syslog(LOG_INFO, "exporting on %s", ep_export);
    } catch(const zmq::error_t& e) {
        syslog(LOG_EMERG, "cannot bind to %s: %s", ep_export, e.what());
        exit(EXIT_FAILURE);
    }

    // Socket for event collection
    s_events.bind("inproc://events");
    s_events.setsockopt(ZMQ_HWM, &watermark, sizeof(watermark));

    // Initializing regexps
    regcomp(&r_subscribe, "s ([0-9]+) ([0-9]+) ([a-z]+)://(.*)", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_unsubscribe, "u ([a-z]+://[[:alnum:]]{32})", REG_EXTENDED | REG_NOSUB);
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "shutting down the engines");
    for(engines_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        delete it->second;
    }

    // Destroying regexps
    regfree(&r_subscribe);
    regfree(&r_unsubscribe);
}

// Message types:
// --------------
// * Subscribe - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket. Plugin
//   will be invoked every 'timeout' microsecods, for 'ttl' seconds
//   or forever if ttl = 0:
//   -> s timeout ttl scheme://parameters
//   <- ok|e scheme|e resources
//
// * Unsubscribe - shuts down the specified thread.
//   Remaining messages will stay orphaned in the queue,
//   so it's a good idea to drain it after the unsubscription:
//   -> u uri
//   <- ok|e uri
//
// * [!] Once - one-time plugin invocation. For one-time invocations,
//   you have no means to filter the data by categories or fields:
//   -> once scheme://parameters
//   <- key=value key2=value2... @timestamp|e scheme
//
// * [!] History - fetch historical data without plugin invocation. 
//   You can't fetch more messages than there were invocations:
//   -> history depth scheme://parameters
//   <- n-part once-like message, where n = max(depth, history-length)
//
// Publishing format:
// ------------------
//   uri key=value @timestamp

void core_t::dispatch(const std::string& request) {
    std::string cmd;
    std::istringstream fmt(request);

    fmt >> std::skipws >> cmd;

    if(regexec(&r_subscribe, request.c_str(), 0, 0, 0) == 0) {
        // 2.1. Subscription
        std::string uri;
        time_t interval, ttl;
        
        fmt >> interval >> ttl >> uri;
        subscribe(uri, interval, ttl);
   
        return;
    }

    if(regexec(&r_unsubscribe, request.c_str(), 0, 0, 0) == 0) {
        // 2.2. Unsubscription
        std::string key;
        
        fmt >> key;
        unsubscribe(key);
        
        return;
    }

    syslog(LOG_ERR, "got an unsupported request: %s", request.c_str());
    respond("e request");
}

void core_t::subscribe(const std::string& uri, time_t interval, time_t ttl) {
    // 3.1. Generating the key
    std::string scheme(uri.substr(0, uri.find_first_of(':')));
    std::string key = scheme + "://" + m_keygen.get(uri);
    
    // 3.2. Search for the engine
    engines_t::iterator it = m_engines.find(key);

    if(it != m_engines.end()) {
        // 3.3a. Increment reference counter and update timers
        it->second->subscribe(interval, ttl);
    } else {
        // 3.3b. Start a new engine
        try {
            engine_t* engine = new engine_t(key, theRegistry->create(scheme, uri), m_context, interval, ttl);
            m_engines.insert(std::make_pair(key, engine));
            syslog(LOG_DEBUG, "created a new engine with uri: %s, interval: %lu, ttl: %lu",
                uri.c_str(), interval, ttl);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "thread creation failed: %s", e.what());
            respond("e runtime");
            return;
        } catch(const std::invalid_argument& e) {
            syslog(LOG_ERR, "invalid uri: %s", e.what());
            respond("e arguments");
            return;
        } catch(const std::domain_error& e) {
            syslog(LOG_ERR, "unknown source type: %s", e.what());
            respond("e source");
            return;
        }
    }

    // 3.4. Return the key
    respond(key);
}

void core_t::unsubscribe(const std::string& key) {
    // 4.1. Search for the engine 
    engines_t::iterator it = m_engines.find(key);
    if(it == m_engines.end()) {
        syslog(LOG_ERR, "got an invalid key: %s", key.c_str());
        respond("e key");
        return;
    }

    // 4.2. Decrement reference counter
    it->second->unsubscribe();

    respond("ok");
}

void core_t::respond(const std::string& response) {
    zmq::message_t message(response.length());
    memcpy(message.data(), response.data(), response.length()); 

    s_requests.send(message, ZMQ_NOBLOCK);
}
