#pragma once

#include "cocaine/api/executor.hpp"

#include <asio/io_service.hpp>

#include <boost/optional/optional.hpp>
#include <boost/thread/thread.hpp>

namespace cocaine {
namespace executor {

// Runs asio::io_service in a spawned thread and posts all callbacks provided to spawn in it.
class owning_asio_t: public api::executor_t {
public:
    enum class stop_policy_t {
        graceful,
        force
    };

    // Choose wheteher to wait wor async operations completion in dtor or not
    owning_asio_t(stop_policy_t stop_policy = stop_policy_t::graceful);

    ~owning_asio_t();

    auto
    spawn(work_t work) -> void override;

    auto
    asio() -> asio::io_service&;

private:
    asio::io_service io_loop;
    boost::optional<asio::io_service::work> work;
    boost::thread thread;
    stop_policy_t stop_policy;
};

// Posts callbacks passed to spawn to externally provided io_service
class borrowing_asio_t: public api::executor_t {
public:
    borrowing_asio_t(asio::io_service& io_loop);

    auto
    spawn(work_t work) -> void override;

private:
    asio::io_service& io_loop;
};

} // namespace executor
} // namespace cocaine
