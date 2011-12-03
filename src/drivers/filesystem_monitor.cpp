#include "cocaine/drivers/filesystem_monitor.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;

filesystem_monitor_t::filesystem_monitor_t(engine_t* engine, const std::string& method, const Json::Value& args):
    driver_t(engine, method),
    m_path(args.get("path", "").asString())
{
    if(m_path.empty()) {
        throw std::runtime_error("no path has been specified for '" + m_method + "' task");
    }
    
    m_watcher.set<filesystem_monitor_t, &filesystem_monitor_t::event>(this);
    m_watcher.start(m_path.c_str());
}

filesystem_monitor_t::~filesystem_monitor_t() {
    m_watcher.stop();
}

Json::Value filesystem_monitor_t::info() const {
    Json::Value result(Json::objectValue);

    result["statistics"] = stats();
    result["type"] = "filesystem-monitor";
    result["path"] = m_path;

    return result;
}

void filesystem_monitor_t::event(ev::stat&, int) {
    boost::shared_ptr<publication_t> job(new publication_t(this));
    m_engine->enqueue(job);
}

