/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/isolates/process.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <csignal>

#include <sys/types.h>
#include <sys/wait.h>

using namespace cocaine;
using namespace cocaine::isolate;

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
        pid_t m_pid;
    };
}

process_t::process_t(context_t& context,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, name, args),
    m_log(context.log(name))
{ }

std::unique_ptr<api::handle_t>
process_t::spawn(const std::string& path,
                 const std::map<std::string, std::string>& args,
                 const std::map<std::string, std::string>& environment)
{
    pid_t pid = ::fork();

    if(pid == 0) {
        char * argv[args.size() * 2 + 2];
        // char * envp[environment.size() + 1];

        // NOTE: The first element is the executable path,
        // the last one should be null pointer.
        argv[0] = ::strdup(path.c_str());
        argv[sizeof(argv) / sizeof(argv[0]) - 1] = NULL;

        // NOTE: The last element of the environment must be a null pointer.
        // envp[sizeof(envp) / sizeof(envp[0]) - 1] = NULL;

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

        // TODO: Merge the current environment and the passed one.
        // It's disabled for now, as will only be needed in forking slave.
        int rv = ::execv(argv[0], argv);

        if(rv != 0) {
            char buffer[1024],
                 * message;

#ifdef _GNU_SOURCE
            message = ::strerror_r(errno, buffer, 1024);
#else
            ::strerror_r(errno, buffer, 1024);

            // NOTE: XSI-compliant strerror_r() returns int instead of the
            // string buffer, so complete the job manually.
            message = buffer;
#endif

            COCAINE_LOG_ERROR(
                m_log,
                "unable to execute '%s' - %s",
                path,
                message
            );

            std::exit(EXIT_FAILURE);
        }
    } else if(pid < 0) {
        throw system_error_t("fork() failed");
    }
    
    return std::unique_ptr<api::handle_t>(new process_handle_t(pid));
}
