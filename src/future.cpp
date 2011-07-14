#include "future.hpp"

using namespace yappi::core;

std::string future_t::seal() {
    syslog(LOG_DEBUG, "future sealed, id: %s", m_id.get().c_str());
    
    Json::FastWriter writer;
    std::string result = writer.write(m_root);

    return result;
}
