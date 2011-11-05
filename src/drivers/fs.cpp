#include "cocaine/drivers/fs.hpp"

using namespace cocaine::engine::drivers;

fs_t::fs_t(engine_t* engine, const std::string& method, const Json::Value& args):
    driver_t(engine, method),
    m_path(args.get("path", "").asString())
{
    if(m_path.empty()) {
        throw std::runtime_error("no path has been specified for '" + m_method + "' task");
    }
    
    m_watcher.set(this);
    m_watcher.start(m_path.c_str());
}

void fs_t::suspend() {
    m_watcher.stop();
}

void fs_t::resume() {
    m_watcher.start();
}

Json::Value fs_t::info() const {
    Json::Value result(Json::objectValue);

    result["type"] = "fs";
    result["path"] = m_path;

    return result;
}

void fs_t::operator()(ev::stat&, int) {
    boost::shared_ptr<publication_t> deferred(new publication_t(this));

    try {
        deferred->enqueue();
    } catch(const resource_error_t& e) {
        syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
            m_engine->name().c_str(), m_method.c_str(), e.what());
        deferred->abort(resource_error, e.what());
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
            m_engine->name().c_str(), m_method.c_str(), e.what());
        deferred->abort(server_error, e.what());
    }
}

