#include "cocaine/detail/service/node/slave/fetcher.hpp"

#include "cocaine/detail/service/node/slave.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

fetcher_t::fetcher_t(std::shared_ptr<state_machine_t> slave_):
    slave(std::move(slave_)),
    watcher(slave->loop)
{}

void
fetcher_t::assign(int fd) {
    watcher.assign(fd);

    COCAINE_LOG_DEBUG(slave->log, "slave has started fetching standard output");
    watch();
}

void
fetcher_t::close() {
    if (watcher.is_open()) {
        COCAINE_LOG_TRACE(slave->log, "slave has cancelled fetching standard output");

        try {
            watcher.close();
        } catch (const std::system_error&) {
            // Eat.
        }
    }
}

void
fetcher_t::watch() {
    COCAINE_LOG_TRACE(slave->log, "slave is fetching more standard output");

    watcher.async_read_some(
        asio::buffer(buffer.data(), buffer.size()),
        std::bind(&fetcher_t::on_read, shared_from_this(), ph::_1, ph::_2)
    );
}

void
fetcher_t::on_read(const std::error_code& ec, size_t len) {
    switch (ec.value()) {
    case 0:
        COCAINE_LOG_TRACE(slave->log, "slave has received %d bytes of output", len);
        slave->output(buffer.data(), len);
        watch();
        break;
    case asio::error::operation_aborted:
        break;
    case asio::error::eof:
        COCAINE_LOG_DEBUG(slave->log, "slave has closed its output");
        break;
    default:
        COCAINE_LOG_WARNING(slave->log, "slave has failed to read output: %s", ec.message());
    }
}
