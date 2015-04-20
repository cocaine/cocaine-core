#include "cocaine/detail/service/node.v2/slave.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node.v2/slave/control.hpp"
#include "cocaine/detail/service/node.v2/util.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

/// The slave's output fetcher.
///
/// \reentrant all methods must be called from the event loop thread, otherwise the behavior is
/// undefined.
class state_machine_t::fetcher_t:
    public std::enable_shared_from_this<fetcher_t>
{
    std::shared_ptr<state_machine_t> slave;

    std::array<char, 4096> buffer;
    asio::posix::stream_descriptor watcher;

public:
    explicit fetcher_t(std::shared_ptr<state_machine_t> slave) :
        slave(slave),
        watcher(slave->loop)
    {}

    /// Assigns an existing native descriptor to the output watcher and starts watching over it.
    ///
    /// \throws std::system_error on any system error while assigning an fd.
    void assign(int fd) {
        watcher.assign(fd);

        COCAINE_LOG_DEBUG(slave->log, "slave has started fetching standard output");
        watch();
    }

    /// Cancels all asynchronous operations associated with the descriptor.
    ///
    /// \throws std::system_error on any system error.
    void cancel() {
        if (watcher.is_open()) {
            COCAINE_LOG_TRACE(slave->log, "slave has cancelled fetching standard output");

            watcher.close();
        }
    }

private:
    void watch() {
        COCAINE_LOG_TRACE(slave->log, "slave is fetching more standard output");

        watcher.async_read_some(
            asio::buffer(buffer.data(), buffer.size()),
            std::bind(&fetcher_t::on_read, shared_from_this(), ph::_1, ph::_2)
        );
    }

    void on_read(const std::error_code& ec, size_t len) {
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
};

class state_machine_t::state_t {
public:
    virtual
    const char*
    name() const noexcept = 0;

    virtual
    bool
    active() const noexcept {
        return false;
    }

    virtual
    size_t
    load() const noexcept {
        return 0;
    }

    /// Cancels all pending asynchronous operations.
    ///
    /// Should be invoked on slave destruction, indicating that the current state should cancel all
    /// its asynchronous operations to break cyclic references.
    // TODO: Consider whether it should be noexcept or not.
    virtual
    void
    cancel() = 0;

    virtual
    std::shared_ptr<control_t>
    activate(std::shared_ptr<session_t> /*session*/, upstream<io::worker::control_tag> /*stream*/) {
        throw std::system_error(error::invalid_state, format("invalid state (%s)", name()));
    }

    virtual
    io::upstream_ptr_t
    inject(std::shared_ptr<const dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>> /*dispatch*/) {
        throw std::system_error(error::invalid_state, format("invalid state (%s)", name()));
    }
};

class state_machine_t::active_t:
    public state_t,
    public std::enable_shared_from_this<active_t>
{
    std::unique_ptr<api::handle_t> handle;
    std::shared_ptr<session_t> session;
    std::shared_ptr<control_t> control;

public:
    active_t(std::shared_ptr<state_machine_t> /*slave*/,
             std::unique_ptr<api::handle_t> handle,
             std::shared_ptr<session_t> session,
             std::shared_ptr<control_t> control):
        handle(std::move(handle)),
        session(std::move(session)),
        control(std::move(control))
    {
        this->control->start();
    }

    ~active_t() {
        if (control) {
            control->cancel();
        }
    }

    const char*
    name() const noexcept {
        return "active";
    }

    virtual
    bool
    active() const noexcept {
        return true;
    }

    virtual
    size_t
    load() const noexcept {
        return session->active_channels().size();
    }

    void
    cancel() {}

    virtual
    io::upstream_ptr_t
    inject(std::shared_ptr<const dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>> d) {
        return session->inject(std::move(d));
    }

    void
    start() {}
};

class state_machine_t::handshaking_t:
    public state_t,
    public std::enable_shared_from_this<handshaking_t>
{
    std::shared_ptr<state_machine_t> slave;

    synchronized<asio::deadline_timer> timer;
    std::unique_ptr<api::handle_t> handle;

    std::chrono::steady_clock::time_point birthtime;

public:
    handshaking_t(std::shared_ptr<state_machine_t> slave, std::unique_ptr<api::handle_t> handle):
        slave(std::move(slave)),
        timer(this->slave->loop),
        handle(std::move(handle)),
        birthtime(std::chrono::steady_clock::now())
    {
        this->slave->fetcher->assign(this->handle->stdout());
    }

    void
    start(unsigned long timeout) {
        COCAINE_LOG_DEBUG(slave->log, "slave is waiting for handshake, timeout: %.2f ms", timeout);

        timer.apply([&](asio::deadline_timer& timer){
            timer.expires_from_now(boost::posix_time::milliseconds(timeout));
            timer.async_wait(std::bind(&handshaking_t::on_timeout, shared_from_this(), ph::_1));
        });
    }

    const char*
    name() const noexcept {
        return "handshaking";
    }

    void
    cancel() {
        std::error_code ec;
        timer->cancel(ec);
    }

    /// Activates the slave by transferring it to the active state using given session and control
    /// channel.
    ///
    /// \threadsafe
    std::shared_ptr<control_t>
    activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
        std::error_code ec;

        const size_t cancelled = timer->cancel(ec);
        if (ec || cancelled == 0) {
            COCAINE_LOG_WARNING(slave->log, "slave has been activated, but the timeout has already expired");
            return nullptr;
        }

        const auto now = std::chrono::steady_clock::now();
        COCAINE_LOG_DEBUG(slave->log, "slave has been activated in %.2f ms",
            std::chrono::duration<float, std::chrono::milliseconds::period>(now - birthtime).count());

        try {
            auto control = std::make_shared<control_t>(slave, std::move(stream));
            auto active = std::make_shared<active_t>(slave, std::move(handle), std::move(session), control);
            slave->migrate(std::move(active));

            active->start();

            return control;
        } catch (const std::exception& err) {
            COCAINE_LOG_ERROR(slave->log, "unable to activate slave: %s", err.what());

            slave->close(error::unknown_activate_error);
        }

        return nullptr;
    }

private:
    void
    on_timeout(const std::error_code& ec) {
        if (!ec) {
            COCAINE_LOG_ERROR(slave->log, "unable to activate slave: timeout");

            slave->close(error::activate_timeout);
        }
    }
};

class state_machine_t::spawning_t:
    public state_machine_t::state_t,
    public std::enable_shared_from_this<spawning_t>
{
    std::shared_ptr<state_machine_t> slave;

    asio::deadline_timer timer;

public:
    explicit spawning_t(std::shared_ptr<state_machine_t> slave) :
        slave(slave),
        timer(slave->loop)
    {}

    void
    spawn(unsigned int timeout) {
        COCAINE_LOG_DEBUG(slave->log, "slave is spawning using '%s', timeout: %.2f ms",
                          slave->context.manifest.executable, timeout);

        COCAINE_LOG_TRACE(slave->log, "locating the Locator endpoint list");
        auto locator = slave->context.context.locate("locator");
        if (!locator || locator->endpoints().empty()) {
            COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: failed to determine the Locator endpoint");

            slave->close(error::locator_not_found);
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
        COCAINE_LOG_TRACE(slave->log, "preparing command line arguments");
        std::map<std::string, std::string> args;
        args["--uuid"]     = slave->context.id;
        args["--app"]      = slave->context.manifest.name;
        args["--endpoint"] = slave->context.manifest.endpoint;
        args["--locator"]  = stream.str();
        args["--protocol"] = std::to_string(io::protocol<io::worker_tag>::version::value);

        // Spawn a worker instance and start reading standard outputs of it.
        try {
            auto isolate = slave->context.context.get<api::isolate_t>(
                slave->context.profile.isolate.type,
                slave->context.context,
                slave->context.manifest.name,
                slave->context.profile.isolate.args
            );

            COCAINE_LOG_TRACE(slave->log, "spawning");

            auto handle = isolate->spawn(
                slave->context.manifest.executable,
                args,
                slave->context.manifest.environment
            );

            // Currently we spawn all slaves synchronously, but here is the right place to provide
            // a callback funtion to the Isolate.
            // NOTE: The callback must be called from the event loop thread, otherwise the behavior
            // is undefined.
            slave->loop.post(move_handler(std::bind(
                &spawning_t::on_spawn, shared_from_this(), std::move(handle), std::chrono::steady_clock::now()
            )));

            timer.expires_from_now(boost::posix_time::milliseconds(timeout));
            timer.async_wait(std::bind(&spawning_t::on_timeout, shared_from_this(), ph::_1));
        } catch(const std::system_error& err) {
            COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: %s", err.code().message());

            slave->close(err.code());
        }
    }

    const char*
    name() const noexcept {
        return "spawning";
    }

    void
    cancel() {
        std::error_code ec;
        timer.cancel(ec);
    }

private:
    void
    on_spawn(std::unique_ptr<api::handle_t>& handle, std::chrono::steady_clock::time_point start) {
        std::error_code ec;
        const size_t cancelled = timer.cancel(ec);
        if (ec || cancelled == 0) {
            COCAINE_LOG_WARNING(slave->log, "slave has been spawned, but the timeout has already expired");
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        COCAINE_LOG_DEBUG(slave->log, "slave has been spawned in %.2f ms",
            std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count());

        try {
            // May throw system error when failed to assign native descriptor to the fetcher.
            auto handshaking = std::make_shared<handshaking_t>(slave, std::move(handle));
            slave->migrate(handshaking);

            handshaking->start(slave->context.profile.timeout.handshake);
        } catch (const std::exception& err) {
            COCAINE_LOG_ERROR(slave->log, "unable to activate slave: %s", err.what());

            slave->close(error::unknown_activate_error);
        }
    }

    void
    on_timeout(const std::error_code& ec) {
        if (!ec) {
            COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: timeout");

            slave->close(error::spawn_timeout);
        }
    }
};

class state_machine_t::broken_t:
    public state_t
{
    std::error_code ec;

public:
    explicit broken_t(std::error_code ec) : ec(std::move(ec)) {}

    const char*
    name() const noexcept {
        return "broken";
    }

    void
    cancel() {}
};

state_machine_t::state_machine_t(slave_context ctx, asio::io_service& loop, cleanup_handler cleanup):
    log(ctx.context.log(format("slave/%s", ctx.manifest.name), {{ "uuid", ctx.id }})),
    context(ctx),
    loop(loop),
    cleanup(std::move(cleanup)),
    lines(context.profile.crashlog_limit)
{
    COCAINE_LOG_TRACE(log, "slave state machine has been initialized");
}

state_machine_t::~state_machine_t() {
    COCAINE_LOG_TRACE(log, "slave state machine has been destroyed");
}

void
state_machine_t::start() {
    COCAINE_LOG_TRACE(log, "slave state machine is starting");

    fetcher = std::make_shared<fetcher_t>(shared_from_this());

    auto spawning = std::make_shared<spawning_t>(shared_from_this());

    loop.post([=]{
        migrate(spawning);
        spawning->spawn(context.profile.timeout.spawn);
    });
}

void
state_machine_t::stop() {
    COCAINE_LOG_TRACE(log, "slave state machine is stopping");

    auto state = std::move(*this->state.synchronize());

    state->cancel();

    fetcher->cancel();
    fetcher.reset();
}

bool
state_machine_t::active() const noexcept {
    return (*state.synchronize())->active();
}

size_t
state_machine_t::load() const noexcept {
    return (*state.synchronize())->load();
}

std::shared_ptr<control_t>
state_machine_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    auto state = *this->state.synchronize();

    return state->activate(std::move(session), std::move(stream));
}

io::upstream_ptr_t
state_machine_t::inject(std::shared_ptr<const dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>> d) {
    return (*state.synchronize())->inject(std::move(d));
}

void
state_machine_t::output(const char* data, size_t size) {
    splitter.consume(std::string(data, size));
    while (auto line = splitter.next()) {
        lines.push_back(*line);

        if (context.profile.log_output) {
            COCAINE_LOG_DEBUG(log, "slave's output: `%s`", *line);
        }
    }
}

void
state_machine_t::migrate(std::shared_ptr<state_t> desired) {
    state.apply([=](std::shared_ptr<state_t>& state){
        COCAINE_LOG_DEBUG(log, "slave has changed its state from '%s' to '%s'",
                          state ? state->name() : "null", desired->name());

        state = desired;
    });
}

void
state_machine_t::close(std::error_code ec) {
    if (ec) {
        migrate(std::make_shared<broken_t>(ec));
    }

    try {
        cleanup(ec);
    } catch (...) {
        // Just eat an exception, we don't care why the cleanup handler failed to do its job.
    }
}

slave_t::slave_t(slave_context context, asio::io_service& loop, cleanup_handler fn) :
    machine(std::make_shared<state_machine_t>(context, loop, fn))
{
    machine->start();
}

slave_t::~slave_t() {
    // This condition is required, because the class itself is movable.
    if (machine) {
        machine->stop();
    }
}

bool
slave_t::active() const noexcept {
    BOOST_ASSERT(machine);

    return machine->active();
}

size_t
slave_t::load() const noexcept {
    BOOST_ASSERT(machine);

    return machine->load();
}

std::shared_ptr<control_t>
slave_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    BOOST_ASSERT(machine);

    return machine->activate(std::move(session), std::move(stream));
}

io::upstream_ptr_t
slave_t::inject(std::shared_ptr<const dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>> d) {
    BOOST_ASSERT(machine);

    return machine->inject(std::move(d));
}

