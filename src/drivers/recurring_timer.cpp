#include "cocaine/drivers/recurring_timer.hpp"

using namespace cocaine::engine::driver;

recurring_timer_t::recurring_timer_t(engine_t* engine, const std::string& method, const Json::Value& args):
    timer_base_t<recurring_timer_t>(engine, method),
    m_interval(args.get("interval", 0.0f).asInt() / 1000.0f)
{
    if(m_interval <= 0.0f) {
        throw std::runtime_error("no interval has been specified for '" + m_method + "' task");
    }
}

Json::Value recurring_timer_t::info() const {
    Json::Value result(Json::objectValue);

    result["statistics"] = stats();
    result["type"] = "recurring-timer";
    result["interval"] = m_interval;

    return result;
}

ev::tstamp recurring_timer_t::reschedule(ev::tstamp now) {
    return now + m_interval;
}
