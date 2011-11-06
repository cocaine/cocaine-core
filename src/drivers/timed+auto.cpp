#include "cocaine/drivers/timed+auto.hpp"

using namespace cocaine::engine::drivers;

auto_timed_t::auto_timed_t(engine_t* engine, const std::string& method, const Json::Value& args):
    timed_t<auto_timed_t>(engine, method),
    m_interval(args.get("interval", 0).asInt() / 1000.0)
{
    if(m_interval <= 0) {
        throw std::runtime_error("no interval has been specified for '" + m_method + "' task");
    }
}

Json::Value auto_timed_t::info() const {
    Json::Value result(Json::objectValue);

    result["type"] = "timed+auto";
    result["spent"] = m_spent;
    result["interval"] = m_interval;

    return result;
}

ev::tstamp auto_timed_t::reschedule(ev::tstamp now) {
    return now + m_interval;
}
