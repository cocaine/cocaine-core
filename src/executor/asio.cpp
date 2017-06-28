#include "cocaine/executor/asio.hpp"

namespace cocaine {
namespace executor {

owning_asio_t::owning_asio_t(stop_policy_t policy):
    io_loop(),
    work(asio::io_service::work(io_loop)),
    thread([&](){ io_loop.run(); }),
    stop_policy(policy)
{}

owning_asio_t::~owning_asio_t() {
    work.reset();
    if(stop_policy == stop_policy_t::force) {
        io_loop.stop();
    }
    thread.join();
}

auto
owning_asio_t::spawn(work_t work) -> void {
    io_loop.post(std::move(work));
}

auto
owning_asio_t::asio() -> asio::io_service& {
    return io_loop;
}

borrowing_asio_t::borrowing_asio_t(asio::io_service& _io_loop) :
    io_loop(_io_loop)
{}

auto
borrowing_asio_t::spawn(work_t work) -> void {
    io_loop.post(std::move(work));
}

} // namespace executor
} // namespace cocaine
