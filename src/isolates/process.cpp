/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/isolates/process.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <system_error>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace cocaine;
using namespace cocaine::isolate;

namespace fs = boost::filesystem;

namespace {

struct process_handle_t:
    public api::handle_t
{
    process_handle_t(pid_t pid):
        m_pid(pid)
    { }

    virtual
   ~process_handle_t() {
        terminate();
    }

    virtual
    void
    terminate() {
        int status = 0;

        if(::waitpid(m_pid, &status, WNOHANG) == 0) {
            ::kill(m_pid, SIGTERM);
        }
    }

private:
    const pid_t m_pid;
};

}

process_t::process_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_log(new logging::log_t(context, name)),
#if BOOST_VERSION >= 104600
    m_working_directory((fs::path(context.config.path.spool) / name).native())
#else
    m_working_directory((fs::path(context.config.path.spool) / name).string())
#endif
{ }

process_t::~process_t() {
    // Empty.
}

std::unique_ptr<api::handle_t>
process_t::spawn(const std::string& path, const api::string_map_t& args, const api::string_map_t& environment, int pipe) {
    const pid_t pid = ::fork();

    if(pid < 0) {
        throw std::system_error(errno, std::system_category(), "unable to fork");
    }

    if(pid > 0) {
        return std::make_unique<process_handle_t>(pid);
    }

    ::dup2(pipe, STDOUT_FILENO);
    ::dup2(pipe, STDERR_FILENO);

    // Set the correct working directory

    try {
        fs::current_path(m_working_directory);
    } catch(const fs::filesystem_error& e) {
        std::cerr << cocaine::format("unable to change the working directory to '%s' - %s", path, e.what());
        std::_Exit(EXIT_FAILURE);
    }

    // Prepare the command line and the environment

    std::vector<char*> argv = { ::strdup(path.c_str()) }, envp;

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

    sigset_t signals;

    sigfillset(&signals);

    ::sigprocmask(SIG_UNBLOCK, &signals, nullptr);

    // Spawn the slave

    if(::execve(argv[0], argv.data(), envp.data()) != 0) {
        std::error_code ec(errno, std::system_category());
        std::cerr << cocaine::format("unable to execute '%s' - %s", path, ec.message());
    }

    std::_Exit(EXIT_FAILURE);
}
