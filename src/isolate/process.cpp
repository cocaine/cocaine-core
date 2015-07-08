/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/detail/isolate/process.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <array>
#include <iostream>

#include <csignal>

#include <boost/filesystem/operations.hpp>
#include <boost/system/system_error.hpp>

#ifdef COCAINE_ALLOW_CGROUPS
    #include <boost/lexical_cast.hpp>
    #include <libcgroup.h>
#endif

#include <sys/wait.h>

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::isolate;

namespace fs = boost::filesystem;

namespace {

class process_terminator_t:
    public std::enable_shared_from_this<process_terminator_t>
{
public:
    const std::unique_ptr<logging::log_t> log;

private:
    pid_t pid;

    struct {
        uint kill;
        uint gc;
    } timeout;

    asio::deadline_timer timer;

public:
    process_terminator_t(pid_t pid_,
                         uint kill_timeout,
                         std::unique_ptr<logging::log_t> log_,
                         asio::io_service& loop):
        log(std::move(log_)),
        pid(pid_),
        timer(loop)
    {
        timeout.kill = kill_timeout;
        timeout.gc   = 5;
    }

    ~process_terminator_t() {
        COCAINE_LOG_TRACE(log, "process terminator is destroying");

        if (pid) {
            int status = 0;

            switch (::waitpid(pid, &status, WNOHANG)) {
            case -1: {
                // Some error occurred, check errno.
                const int ec = errno;

                COCAINE_LOG_WARNING(log, "unable to properly collect the child: %d", ec);
            }
                break;
            case 0:
                // The child is not finished yet, kill it and collect in a blocking way as as last
                // resort to prevent zombies.
                if (::kill(pid, SIGKILL) == 0) {
                    if (::waitpid(pid, &status, 0) > 0) {
                        COCAINE_LOG_TRACE(log, "child has been killed: %d", status);
                    } else {
                        const int ec = errno;

                        COCAINE_LOG_WARNING(log, "unable to properly collect the child: %d", ec);
                    }
                } else {
                    // Unable to kill for some reasons, check errno.
                    const int ec = errno;

                    COCAINE_LOG_WARNING(log, "unable to send kill signal to the child: %d", ec);
                }
                break;
            default:
                COCAINE_LOG_TRACE(log, "child has been collected: %d", status);
            }
        }
    }

    void
    start() {
        int status = 0;

        // Attempt to collect the child non-blocking way.
        switch (::waitpid(pid, &status, WNOHANG)) {
        case -1: {
            const int ec = errno;

            COCAINE_LOG_WARNING(log, "unable to collect the child: %d", ec);
            break;
        }
        case 0: {
            // The child is not finished yet, send SIGTERM and try to collect it later after.
            COCAINE_LOG_TRACE(log, "unable to terminate child right now (not ready), sending SIGTERM")(
                "timeout", timeout.kill
            );

            // Ignore return code here.
            ::kill(pid, SIGTERM);

            timer.expires_from_now(boost::posix_time::seconds(timeout.kill));
            timer.async_wait(std::bind(&process_terminator_t::on_kill_timer, shared_from_this(), ph::_1));
            break;
        }
        default:
            COCAINE_LOG_TRACE(log, "child has been stopped: %d", status);

            pid = 0;
        }
    }

private:
    void
    on_kill_timer(const std::error_code& ec) {
        if(ec == asio::error::operation_aborted) {
            COCAINE_LOG_TRACE(log, "process kill timer has called its completion handler: cancelled");
            return;
        } else {
            COCAINE_LOG_TRACE(log, "process kill timer has called its completion handler");
        }

        int status = 0;

        switch (::waitpid(pid, &status, WNOHANG)) {
        case -1: {
            const int ec = errno;

            COCAINE_LOG_WARNING(log, "unable to collect the child: %d", ec);
            break;
        }
        case 0: {
            COCAINE_LOG_TRACE(log, "killing the child, resuming after 5 sec");

            // Ignore return code here too.
            ::kill(pid, SIGKILL);

            timer.expires_from_now(boost::posix_time::seconds(timeout.gc));
            timer.async_wait(std::bind(&process_terminator_t::on_gc_action, shared_from_this(), ph::_1));
            break;
        }
        default:
            COCAINE_LOG_TRACE(log, "child has been terminated: %d", status);

            pid = 0;
        }
    }

    void
    on_gc_action(const std::error_code& ec) {
        if(ec == asio::error::operation_aborted) {
            COCAINE_LOG_TRACE(log, "process GC timer has called its completion handler: cancelled");
            return;
        } else {
            COCAINE_LOG_TRACE(log, "process GC timer has called its completion handler");
        }

        int status = 0;

        switch (::waitpid(pid, &status, WNOHANG)) {
        case -1: {
            const int ec = errno;

            COCAINE_LOG_WARNING(log, "unable to collect the child: %d", ec);
            break;
        }
        case 0: {
            COCAINE_LOG_TRACE(log, "child has not been killed, resuming after 5 sec");

            timer.expires_from_now(boost::posix_time::seconds(timeout.gc));
            timer.async_wait(std::bind(&process_terminator_t::on_gc_action, shared_from_this(), ph::_1));
            break;
        }
        default:
            COCAINE_LOG_TRACE(log, "child has been killed: %d", status);

            pid = 0;
        }
    }
};

struct process_handle_t:
    public api::handle_t
{
private:
    std::shared_ptr<process_terminator_t> terminator;

    const int m_stdout;

public:
    process_handle_t(pid_t pid,
                     int stdout,
                     uint kill_timeout,
                     std::unique_ptr<logging::log_t> log,
                     asio::io_service& loop):
        terminator(std::make_shared<process_terminator_t>(pid, kill_timeout, std::move(log), loop)),
        m_stdout(stdout)
    {
        COCAINE_LOG_TRACE(terminator->log, "process handle has been created");
    }

    ~process_handle_t() {
        terminate();
        COCAINE_LOG_TRACE(terminator->log, "process handle has been destroyed");
    }

    virtual
    void
    terminate() {
        terminator->start();

        ::close(m_stdout);
    }

    virtual
    int
    stdout() const {
        return m_stdout;
    }
};

#ifdef COCAINE_ALLOW_CGROUPS
struct cgroup_configurator_t:
    public boost::static_visitor<void>
{
    cgroup_configurator_t(cgroup_controller* ptr_, const char* parameter_):
        ptr(ptr_),
        parameter(parameter_)
    { }

    void
    operator()(const dynamic_t::bool_t& value) const {
        cgroup_add_value_bool(ptr, parameter, value);
    }

    void
    operator()(const dynamic_t::int_t& value) const {
        cgroup_add_value_int64(ptr, parameter, value);
    }

    void
    operator()(const dynamic_t::uint_t& value) const {
        cgroup_add_value_uint64(ptr, parameter, value);
    }

    void
    operator()(const dynamic_t::string_t& value) const {
        cgroup_add_value_string(ptr, parameter, value.c_str());
    }

    template<class T>
    void
    operator()(const T& COCAINE_UNUSED_(value)) const {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument));
    }

private:
    cgroup_controller *const ptr;

    // Parameter name is something like "cpu.shares" or "blkio.weight", i.e. it includes the name of
    // the actual controller it corresponds to.
    const char* parameter;
};

struct cgroup_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.isolate.process.cgroups";
    }

    virtual
    auto
    message(int code) const -> std::string {
        return cgroup_strerror(code);
    }
};
#endif

} // namespace

#ifdef COCAINE_ALLOW_CGROUPS
namespace cocaine { namespace error {

auto
cgroup_category() -> const std::error_category& {
    static cgroup_category_t instance;
    return instance;
}

}} // namespace cocaine::error
#endif

process_t::process_t(context_t& context, asio::io_service& io_context, const std::string& name, const dynamic_t& args):
    category_type(context, io_context, name, args),
    m_context(context),
    io_context(io_context),
    m_log(context.log(name)),
    m_name(name),
    m_working_directory(fs::path(args.as_object().at("spool", "/var/spool/cocaine").as_string()) / name),
    m_kill_timeout(args.as_object().at("kill_timeout", 30u).as_uint())
{
#ifdef COCAINE_ALLOW_CGROUPS
    int rv = 0;

    if((rv = cgroup_init()) != 0) {
        throw std::system_error(rv, error::cgroup_category(), "unable to initialize cgroups");
    }

    m_cgroup = cgroup_new_cgroup(m_name.c_str());

    // NOTE: Looks like if this is not done, then libcgroup will chown everything as root.
    cgroup_set_uid_gid(m_cgroup, getuid(), getgid(), getuid(), getgid());

    for(auto type = args.as_object().begin(); type != args.as_object().end(); ++type) {
        if(!type->second.is_object() || type->second.as_object().empty()) {
            continue;
        }

        cgroup_controller* ptr = cgroup_add_controller(m_cgroup, type->first.c_str());

        for(auto it = type->second.as_object().begin(); it != type->second.as_object().end(); ++it) {
            COCAINE_LOG_INFO(m_log, "setting cgroup controller '%s' parameter '%s' to '%s'",
                type->first, it->first, boost::lexical_cast<std::string>(it->second)
            );

            try {
                it->second.apply(cgroup_configurator_t(ptr, it->first.c_str()));
            } catch(const std::system_error& e) {
                COCAINE_LOG_ERROR(m_log, "unable to set cgroup controller '%s' parameter '%s' - %s",
                    type->first, it->first, e.what()
                );
            }
        }
    }

    if((rv = cgroup_create_cgroup(m_cgroup, false)) != 0) {
        cgroup_free(&m_cgroup);

        throw std::system_error(rv, error::cgroup_category(), "unable to create cgroup");
    }
#endif
}

process_t::~process_t() {
#ifdef COCAINE_ALLOW_CGROUPS
    int rv = 0;

    if((rv = cgroup_delete_cgroup(m_cgroup, false)) != 0) {
        COCAINE_LOG_ERROR(m_log, "unable to delete cgroup: %s", cgroup_strerror(rv));
    }

    cgroup_free(&m_cgroup);
#endif
}

#ifdef __APPLE__
    #include <crt_externs.h>
    #define environ (*_NSGetEnviron())
#else
    extern char** environ;
#endif

std::unique_ptr<api::handle_t>
process_t::spawn(const std::string& path, const api::string_map_t& args, const api::string_map_t& environment) {
    std::array<int, 2> pipes;

    if(::pipe(pipes.data()) != 0) {
        throw std::system_error(errno, std::system_category(), "unable to create an output pipe");
    }

    for(auto it = pipes.begin(); it != pipes.end(); ++it) {
        ::fcntl(*it, F_SETFD, FD_CLOEXEC);
    }

    const pid_t pid = ::fork();

    if(pid < 0) {
        std::for_each(pipes.begin(), pipes.end(), ::close);

        throw std::system_error(errno, std::system_category(), "unable to fork");
    }

    ::close(pipes[pid > 0]);

    if(pid > 0) {
        return std::make_unique<process_handle_t>(
            pid,
            pipes[0],
            m_kill_timeout,
            m_context.log(format("%s/process", m_name), {{ "pid", blackhole::attribute::value_t(pid) }}),
            io_context
        );
    }

    // Child initialization

    ::dup2(pipes[1], STDOUT_FILENO);
    ::dup2(pipes[1], STDERR_FILENO);

#ifdef COCAINE_ALLOW_CGROUPS
    // Attach to the control group

    int rv = 0;

    if((rv = cgroup_attach_task(m_cgroup)) != 0) {
        std::cerr << cocaine::format("unable to attach the process to a cgroup - %s", cgroup_strerror(rv));
        std::_Exit(EXIT_FAILURE);
    }
#endif

    // Set the correct working directory

    try {
        fs::current_path(m_working_directory);
    } catch(const fs::filesystem_error& e) {
        std::cerr << cocaine::format("unable to change the working directory to '%s' - %s", m_working_directory, e.what());
        std::_Exit(EXIT_FAILURE);
    }

    // Prepare the command line and the environment

    auto target = fs::path(path);

#if BOOST_VERSION >= 104600
    if(!target.is_absolute()) {
#else
    if(!target.is_complete()) {
#endif
        target = m_working_directory / target;
    }

#if BOOST_VERSION >= 104600
    std::vector<char*> argv = { ::strdup(target.native().c_str()) }, envp;
#else
    std::vector<char*> argv = { ::strdup(target.string().c_str()) }, envp;
#endif

    for(auto it = args.begin(); it != args.end(); ++it) {
        argv.push_back(::strdup(it->first.c_str()));
        argv.push_back(::strdup(it->second.c_str()));
    }

    argv.push_back(nullptr);

    for(char** ptr = environ; *ptr != nullptr; ++ptr) {
        envp.push_back(::strdup(*ptr));
    }

    boost::format format("%s=%s");

    for(auto it = environment.begin(); it != environment.end(); ++it, format.clear()) {
        envp.push_back(::strdup((format % it->first % it->second).str().c_str()));
    }

    envp.push_back(nullptr);

    // Unblock all the signals

    sigset_t sigset;

    sigfillset(&sigset);

    ::sigprocmask(SIG_UNBLOCK, &sigset, nullptr);

    // Spawn the slave

    if(::execve(argv[0], argv.data(), envp.data()) != 0) {
        std::error_code ec(errno, std::system_category());
        std::cerr << cocaine::format("unable to execute '%s' - [%d] %s", path, ec.value(), ec.message());
    }

    std::_Exit(EXIT_FAILURE);
}
