#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

context_t::context_t(config_t config_, boost::shared_ptr<logging::sink_t> sink):
    config(config_),
    m_io(new zmq::context_t(1)),
    m_log(sink)
{
    // Fetching the hostname
    const int HOSTNAME_MAX_LENGTH = 256;
    char hostname[HOSTNAME_MAX_LENGTH];

    if(gethostname(hostname, HOSTNAME_MAX_LENGTH) == 0) {
        config.core.hostname = hostname;
    } else {
        throw std::runtime_error("failed to determine the hostname");
    }
}

