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
    pid_t pid = ::fork();

    if(pid < 0) {
        throw std::system_error(errno, std::system_category(), "unable to fork");
    }

    if(pid == 0) {
        ::dup2(pipe, STDOUT_FILENO);
        ::dup2(pipe, STDERR_FILENO);

        // Prepare the arguments and environment

        size_t argc = args.size() * 2 + 2;
        // size_t envc = environment.size() + 1;

        char** argv = new char* [argc];
        // char** envp[] = new char* [envc];

        // NOTE: The first element is the executable path, the last one should be null pointer.
        argv[0] = ::strdup(path.c_str());
        argv[argc - 1] = nullptr;

        // NOTE: The last element of the environment must be a null pointer.
        // envp[envc - 1] = nullptr;

        std::map<std::string, std::string>::const_iterator it;
        int n;

        it = args.begin();
        n = 1;

        while(it != args.end()) {
            argv[n++] = ::strdup(it->first.c_str());
            argv[n++] = ::strdup(it->second.c_str());

            ++it;
        }

        if(!environment.empty()) {
            COCAINE_LOG_WARNING(m_log, "environment passing is not implemented");
        }

        /*
        boost::format format("%s=%s");

        it = environment.begin();
        n = 0;

        while(it != environment.end()) {
            format % it->first % it->second;

            envp[n++] = ::strdup(format.str().c_str());

            format.clear();
            ++it;
        }
        */

        try {
            fs::current_path(m_working_directory);
        } catch(const fs::filesystem_error& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to change the working directory to '%s' - %s",
                path,
                e.what()
            );

            std::_Exit(EXIT_FAILURE);
        }

        // Unblock all the signals

        sigset_t signals;

        sigfillset(&signals);

        ::sigprocmask(SIG_UNBLOCK, &signals, nullptr);

        // Spawn the slave

        if(::execv(argv[0], argv) != 0) {
            std::error_code ec(errno, std::system_category());

            COCAINE_LOG_ERROR(
                m_log,
                "unable to execute '%s' - %s",
                path,
                ec.message()
            );

            std::_Exit(EXIT_FAILURE);
        }
    }

    return std::unique_ptr<api::handle_t>(new process_handle_t(pid));
}
