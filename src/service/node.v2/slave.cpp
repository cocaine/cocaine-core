#include "cocaine/detail/service/node.v2/slave.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node.v2/util.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

class slave_t::fetcher_t:
    public std::enable_shared_from_this<fetcher_t>
{
    slave_t& slave;

    std::array<char, 4096> buffer;
    asio::posix::stream_descriptor watcher;

public:
    explicit fetcher_t(slave_t& slave) :
        slave(slave),
        watcher(slave.loop)
    {}

    void assign(int fd) {
        COCAINE_LOG_DEBUG(slave.log, "slave has started fetching standard output");

        watcher.assign(fd);
        watch();
    }

    void watch() {
        COCAINE_LOG_TRACE(slave.log, "slave is fetching more standard output");

        watcher.async_read_some(
            asio::buffer(buffer.data(), buffer.size()),
            std::bind(&fetcher_t::on_read, shared_from_this(), ph::_1, ph::_2)
        );
    }

    void cancel() {
        watcher.cancel();
    }

private:
    void on_read(const std::error_code& ec, size_t len) {
        switch (ec.value()) {
        case 0:
            COCAINE_LOG_TRACE(slave.log, "slave has received %d bytes of output", len);
            slave.output(buffer.data(), len);
            watch();
            break;
        case asio::error::operation_aborted:
            break;
        case asio::error::eof:
            COCAINE_LOG_DEBUG(slave.log, "slave has closed its output");
            break;
        default:
            COCAINE_LOG_WARNING(slave.log, "slave has failed to read output: %s", ec.message());
            // TODO: Something abnormal happened. Maybe terminate itself?
        }
    }
};

class slave_t::state_t {
public:
    virtual
    const char*
    name() const noexcept = 0;

    /// Cancels all pending asynchronous operations.
    ///
    /// Should be invoked on slave destruction, indicating that the current state should cancel all
    /// its asynchronous operations to break cyclic references.
    virtual
    void cancel() = 0;
};

class slave_t::unauthenticated_t:
    public slave_t::state_t,
    public std::enable_shared_from_this<unauthenticated_t>
{
    slave_t& slave;

    asio::deadline_timer timer;
    std::unique_ptr<api::handle_t> handle;

public:
    unauthenticated_t(slave_t& slave, std::unique_ptr<api::handle_t> handle):
        slave(slave),
        timer(slave.loop),
        handle(std::move(handle))
    {
        slave.fetcher->assign(this->handle->stdout());
    }

    void
    start(unsigned long timeout) {
        COCAINE_LOG_DEBUG(slave.log, "slave is waiting for handshake, timeout: %.2f ms", timeout);

        timer.expires_from_now(boost::posix_time::milliseconds(timeout));
        timer.async_wait(std::bind(&unauthenticated_t::on_timeout, shared_from_this(), ph::_1));
    }

    const char*
    name() const noexcept {
        return "unauthenticated";
    }

    void cancel() {
        handle->terminate();
        timer.cancel();
    }

private:
    void
    on_timeout(const std::error_code& ec) {
        if (!ec) {
            COCAINE_LOG_ERROR(slave.log, "unable to activate slave: timeout");

            // TODO: Make unique category.
            slave.close(asio::error::timed_out);
        }
    }
};

class slave_t::spawning_t:
    public slave_t::state_t,
    public std::enable_shared_from_this<spawning_t>
{
    slave_t& slave;

    asio::deadline_timer timer;

public:
    explicit spawning_t(slave_t& slave) :
        slave(slave),
        timer(slave.loop)
    {}

    void
    spawn(unsigned int timeout) {
        COCAINE_LOG_DEBUG(slave.log, "slave is spawning using '%s', timeout: %.2f ms",
                          slave.context.manifest.executable, timeout);

        auto locator = slave.context.context.locate("locator");
        if (!locator || locator->endpoints().empty()) {
            COCAINE_LOG_ERROR(slave.log, "unable to spawn slave: failed to determine the Locator endpoint");

            // TODO: Make unique category.
            slave.close(asio::error::host_not_found);
            return;
        }

        // Fill Locator's endpoint list.
        std::ostringstream stream;
        const auto& endpoints = locator->endpoints();
        auto it = endpoints.begin();
        stream << *it;
        ++it;

        for (; it != endpoints.end(); ++it) {
            stream << "," << *it;
        }

        // Prepare command line arguments for worker instance.
        COCAINE_LOG_TRACE(slave.log, "preparing command line arguments");
        std::map<std::string, std::string> args;
        args["--uuid"]     = slave.context.id;
        args["--app"]      = slave.context.manifest.name;
        args["--endpoint"] = slave.context.manifest.endpoint;
        args["--locator"]  = stream.str();
        args["--protocol"] = std::to_string(io::protocol<io::worker_tag>::version::value);

        // Spawn a worker instance and start reading standard outputs of it.
        try {
            auto isolate = slave.context.context.get<api::isolate_t>(
                slave.context.profile.isolate.type,
                slave.context.context,
                slave.context.manifest.name,
                slave.context.profile.isolate.args
            );

            COCAINE_LOG_TRACE(slave.log, "spawning");

            auto handle = isolate->spawn(
                slave.context.manifest.executable,
                args,
                slave.context.manifest.environment
            );

            // Currently we spawn all slaves syncronously.
            slave.loop.post(move_handler(std::bind(
                &spawning_t::on_spawn, shared_from_this(), std::move(handle), std::chrono::steady_clock::now()
            )));

            timer.expires_from_now(boost::posix_time::milliseconds(timeout));
            timer.async_wait(std::bind(&spawning_t::on_timeout, shared_from_this(), ph::_1));
        } catch(const std::system_error& err) {
            COCAINE_LOG_ERROR(slave.log, "unable to spawn slave: %s", err.code().message());

            slave.close(err.code());
        }
    }

    const char*
    name() const noexcept {
        return "spawning";
    }

    void
    cancel() {
        timer.cancel();
    }

private:
    void
    on_spawn(std::unique_ptr<api::handle_t>& handle, std::chrono::steady_clock::time_point start) {
        const auto now = std::chrono::steady_clock::now();
        COCAINE_LOG_DEBUG(slave.log, "slave has been spawned in %.2f ms",
            std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count());

        timer.cancel();
        auto unauthenticated = std::make_shared<unauthenticated_t>(slave, std::move(handle));
        unauthenticated->start(slave.context.profile.timeout.handshake);
        slave.migrate(std::move(unauthenticated));
    }

    void
    on_timeout(const std::error_code& ec) {
        if (!ec) {
            COCAINE_LOG_ERROR(slave.log, "unable to spawn slave: timeout");

            // TODO: Make unique category.
            slave.close(asio::error::timed_out);
        }
    }
};

class slave_t::broken_t:
    public slave_t::state_t
{
    std::error_code ec;

public:
    explicit broken_t(std::error_code ec) : ec(ec) {}

    const char*
    name() const noexcept {
        return "broken";
    }

    void
    cancel() {}
};

slave_t::slave_t(slave_context ctx, asio::io_service& loop, cleanup_handler fn) :
    log(ctx.context.log(format("slave/%s", ctx.manifest.name), {{ "uuid", ctx.id }})),
    context(std::move(ctx)),
    loop(loop),
    cleanup(std::move(fn)),
    fetcher(std::make_shared<fetcher_t>(*this)),
    lines(context.profile.crashlog_limit)
{
    auto spawning = std::make_shared<spawning_t>(*this);
    state.unsafe() = spawning;

    loop.post([=]{
        spawning->spawn(context.profile.timeout.spawn);
    });
}

slave_t::~slave_t() {
    state.apply([](std::shared_ptr<state_t> state){
        state->cancel();
    });

    fetcher->cancel();
}

void
slave_t::activate(std::shared_ptr<session_t> /*session*/, std::shared_ptr<control_t> /*control*/) {

}

void
slave_t::output(const char* data, size_t size) {
    splitter.consume(std::string(data, size));
    while (auto line = splitter.next()) {
        lines.push_back(*line);

        if (context.profile.log_output) {
            COCAINE_LOG_DEBUG(log, "slave's output: `%s`", *line);
        }
    }
}

void
slave_t::migrate(std::shared_ptr<slave_t::state_t> desired) {
    state.apply([=](std::shared_ptr<state_t>& state){
        COCAINE_LOG_DEBUG(log, "slave has changed its state from '%s' to '%s'", state->name(), desired->name());

        state = desired;
    });
}

void
slave_t::close(std::error_code ec) {
    if (ec) {
        migrate(std::make_shared<broken_t>(ec));
    }

    cleanup(ec);
}
