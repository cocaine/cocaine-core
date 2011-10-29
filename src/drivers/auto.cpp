#include "cocaine/drivers/auto.hpp"

using namespace cocaine::engine::drivers;

auto_t::auto_t(const std::string& method, boost::shared_ptr<engine_t> parent, const Json::Value& args):
    timed_driver_t<auto_t>(method, parent),
    m_interval(args.get("interval", 0).asInt() / 1000.0)
{
    if(m_interval <= 0) {
        throw std::runtime_error("no interval has been specified");
    }
}

Json::Value auto_t::info() const {
    Json::Value result(Json::objectValue);

    result["type"] = "auto";
    result["interval"] = m_interval;

    return result;
}

ev::tstamp auto_t::reschedule(ev::tstamp now) {
    return now + m_interval;
}
