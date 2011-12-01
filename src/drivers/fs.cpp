#include "cocaine/drivers/fs.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;

fs_t::fs_t(engine_t* engine, const std::string& method, const Json::Value& args):
    driver_t(engine, method),
    m_path(args.get("path", "").asString())
{
    if(m_path.empty()) {
        throw std::runtime_error("no path has been specified for '" + m_method + "' task");
    }
    
    m_watcher.set<fs_t, &fs_t::event>(this);
    m_watcher.start(m_path.c_str());
}

void fs_t::stop() {
    m_watcher.stop();
}

Json::Value fs_t::info() const {
    Json::Value result(Json::objectValue);

    result["stats"] = stats();
    result["type"] = "fs";
    result["path"] = m_path;

    return result;
}

void fs_t::event(ev::stat&, int) {
    boost::shared_ptr<publication_t> job(new publication_t(this));
    m_engine->enqueue(job);
}

