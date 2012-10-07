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

#include <sys/wait.h>
#include <unistd.h>

#include "cocaine/isolates/process.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::api;
using namespace cocaine::isolate;

namespace {
    class process_handle_t:
        public api::handle_t
    {
        public:
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

process_t::process_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_log(context.log(name))
{ }

std::unique_ptr<handle_t>
process_t::spawn(const std::string& path,
                 const std::map<std::string, std::string>& args)
{
    typedef std::map<
        std::string,
        std::string
    > arg_map_t;
    
    pid_t pid = ::fork();

    if(pid == 0) {
        char * argv[args.size() * 2 + 2];

        // NOTE: First element is the executable path,
        // last one should be null pointer.
        argv[0] = ::strdup(path.c_str());
        argv[sizeof(argv) / sizeof(char*)] = NULL;

        arg_map_t::const_iterator it(args.begin());
        
        // NOTE: Start with the second element.
        int n = 1;
        
        while(it != args.end()) {
            argv[n++] = ::strdup(it->first.c_str());
            argv[n++] = ::strdup(it->second.c_str());
            
            ++it;
        }

        int rv = ::execvp(
            argv[0],
            argv
        );

        if(rv != 0) {
            char buffer[1024];

#ifdef _GNU_SOURCE
            char * message;
            message = ::strerror_r(errno, buffer, 1024);
#else
            ::strerror_r(errno, buffer, 1024);
#endif

            m_log->error(
                "unable to execute '%s' - %s",
                path.c_str(),
#ifdef _GNU_SOURCE
                message
#else
                buffer
#endif
            );

            std::exit(EXIT_FAILURE);
        }
    } else if(pid < 0) {
        throw system_error_t("fork() failed");
    }
    
    return std::unique_ptr<handle_t>(
        new process_handle_t(pid)
    );
}
