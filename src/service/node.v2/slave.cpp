#include "cocaine/detail/service/node.v2/slave.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node.v2/util.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::slave;

class cocaine::slave::fetcher_t : public std::enable_shared_from_this<fetcher_t> {
    std::unique_ptr<logging::log_t> log;

    std::function<void(std::string)> cb;
    std::array<char, 4096> buffer;
    asio::posix::stream_descriptor watcher;

public:
    fetcher_t(context_t& context, slave_data d, int fd, std::shared_ptr<asio::io_service> loop) :
        log(context.log(format("slave/%s/output", d.manifest.name), {{ "uuid", d.id }})),
        cb(d.output),
        watcher(*loop)
    {
        watcher.assign(fd);
    }

    void watch() {
        COCAINE_LOG_TRACE(log, "slave is fetching more standard output");

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
            COCAINE_LOG_TRACE(log, "slave has received %d bytes of output", len);
            cb(std::string(buffer.data(), len));
            watch();
            break;
        case asio::error::operation_aborted:
            break;
        case asio::error::eof:
            COCAINE_LOG_DEBUG(log, "slave has closed its output");
            break;
        default:
            COCAINE_LOG_WARNING(log, "slave has failed to read output: %s", ec.message());
            // TODO: Something abnormal happened. Maybe terminate itself?
        }
    }
};

spawning_t::spawning_t(context_t& context, slave_data d, std::shared_ptr<asio::io_service> loop, spawning_t::callback_type fn) :
    log(context.log(format("slave/%s/spawning", d.manifest.name), {{ "uuid", d.id }})),
    fn(std::move(fn)),
    fired(false)
{
    COCAINE_LOG_DEBUG(log, "slave is spawning using '%s'", d.manifest.executable);

    // TODO: I don't like, that we can possibly create an object in invalid state.
    // Maybe it's a better idea to create this object via a special function?

    // TODO: Hardcoded locator name.
    auto locator = context.locate("locator");
    if (!locator) {
        // TODO: What if there are no locator?
        COCAINE_LOG_ERROR(log, "unable to determine locator endpoint");
        loop->post([=]{ set(std::error_code(asio::error::host_not_found)); });
        return;
    }

    // Prepare command line arguments for worker instance.
    std::map<std::string, std::string> args;
    args["--uuid"]     = d.id;
    args["--app"]      = d.manifest.name;
    args["--endpoint"] = d.manifest.endpoint;
    // TODO: Assign locator endpoints list.
    args["--locator"]  = format("%s:%d", context.config.network.hostname, locator->endpoints().front().port());
    // TODO: Protocol version.
    // args["--protocol"] = std::to_string(io::protocol<io::rpc_tag>::version::value);

    // Spawn a worker instance and start reading standard outputs of it.
    try {
        // TODO: What if there are no isolate?
        auto isolate = context.get<api::isolate_t>(
            d.profile.isolate.type,
            context,
            d.manifest.name,
            d.profile.isolate.args
        );

        auto handle = isolate->spawn(d.manifest.executable, args, d.manifest.environment);
        auto done = [=, &context](std::unique_ptr<api::handle_t>& handle){
            set(std::make_shared<unauthenticated_t>(context, d, loop, std::move(handle)));
        };
        loop->post(move_handler(std::bind(done, std::move(handle))));
    } catch(const std::system_error& err) {
        loop->post([=]{ set(err.code()); });
    }
}

void spawning_t::set(result_type&& res) {
    if (!fired.exchange(true)) {
        fn(std::move(res));
    }
}

void spawning_t::cancel() {
    set(std::error_code(asio::error::operation_aborted));
}

std::shared_ptr<spawning_t>
slave::spawn(context_t& context, slave_data d, std::shared_ptr<asio::io_service> loop, spawning_t::callback_type fn) {
    auto timer = std::make_shared<asio::deadline_timer>(*loop);

    auto slave = std::make_shared<slave::spawning_t>(context, d, loop, [=](result<std::shared_ptr<unauthenticated_t>> result){
        timer->cancel();
        fn(std::move(result));
    });

    timer->expires_from_now(boost::posix_time::milliseconds(d.profile.timeout.spawn));
    timer->async_wait([timer, slave](const std::error_code& ec){
        if (ec.value() == 0) {
            slave->set(std::error_code(asio::error::timed_out));
        }
    });

    return slave;
}

unauthenticated_t::unauthenticated_t(context_t& context, slave_data d, std::shared_ptr<asio::io_service> loop, std::unique_ptr<api::handle_t> handle) :
    log(context.log(format("slave/%s/unauthenticated", d.manifest.name), {{ "uuid", d.id }})),
    d(d),
    fetcher(std::make_shared<fetcher_t>(context, d, handle->stdout(), loop)),
    handle(std::move(handle)),
    timer(*loop),
    start(std::chrono::steady_clock::now())
{
    COCAINE_LOG_DEBUG(log, "slave has started fetching standard output");

    fetcher->watch();
}

void unauthenticated_t::activate_in(std::function<void ()> on_timeout){
    timer.expires_from_now(boost::posix_time::milliseconds(d.profile.timeout.handshake));
    timer.async_wait([=](const std::error_code& ec){
        if (!ec) {
            on_timeout();
        }
    });
}

std::shared_ptr<active_t>
unauthenticated_t::activate(std::shared_ptr<control_t> control, std::shared_ptr<session_t> session) {
    timer.cancel();

    auto now = std::chrono::steady_clock::now();
    COCAINE_LOG_DEBUG(log, "slave has become active in %.3fms",
        std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count()
    );

    return std::make_shared<active_t>(std::move(*this), std::move(control), std::move(session));
}

void unauthenticated_t::terminate() {
    fetcher->cancel();
}

active_t::active_t(unauthenticated_t&& unauth, std::shared_ptr<control_t> control, std::shared_ptr<session_t> session) :
    fetcher(std::move(unauth.fetcher)),
    control(control),
    session(session),
    handle(std::move(unauth.handle))
{}

io::upstream_ptr_t active_t::inject(io::dispatch_ptr_t dispatch) {
    return session->inject(dispatch);
}
