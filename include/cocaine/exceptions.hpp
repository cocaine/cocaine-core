//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef COCAINE_EXCEPTIONS_HPP
#define COCAINE_EXCEPTIONS_HPP

#include <errno.h>
#include <stdexcept>
#include <string.h>

namespace cocaine {

struct authorization_error_t:
    public std::runtime_error
{
    authorization_error_t(const std::string& what):
        std::runtime_error(what)
    { }
};

struct configuration_error_t:
    public std::runtime_error
{
    configuration_error_t(const std::string& what):
        std::runtime_error(what)
    { }
};

struct registry_error_t:
    public std::runtime_error
{
    registry_error_t(const std::string& what):
        std::runtime_error(what)
    { }
};

struct system_error_t:
    public std::runtime_error
{
    public:
        system_error_t(const std::string& what):
            std::runtime_error(what)
        {
            ::strerror_r(errno, m_reason, 1024);
        }

        const char * reason() const {
            return m_reason;
        }

    private:
        char m_reason[1024];
};

struct storage_error_t:
    public std::runtime_error
{
    storage_error_t(const std::string& what):
        std::runtime_error(what)
    { }
};

// Application exceptions
// ----------------------

struct unrecoverable_error_t:
    public std::runtime_error
{
    unrecoverable_error_t(const std::string& what):
        std::runtime_error(what)
    { }
};

struct recoverable_error_t:
    public std::runtime_error
{
    recoverable_error_t(const std::string& what):
        std::runtime_error(what)
    { }
};

}

#endif
