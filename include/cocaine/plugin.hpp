//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_PLUGIN_HPP
#define COCAINE_PLUGIN_HPP

#include <boost/function.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace plugin {

class unrecoverable_error_t:
    public std::runtime_error
{
    public:
        unrecoverable_error_t(const std::string& what):
            std::runtime_error(what)
        { }
};

class recoverable_error_t:
    public std::runtime_error
{
    public:
        recoverable_error_t(const std::string& what):
            std::runtime_error(what)
        { }
};

// A callback function type used to push response chunks
typedef boost::function<void(const void*, size_t)> callback_fn_t;

class source_t:
    public boost::noncopyable
{
    public:
        virtual ~source_t() { };

        virtual void invoke(
            callback_fn_t callback,
            const std::string& method,
            const void* request,
            size_t size) = 0;
};

// Plugins are expected to supply at least one factory function
// to initialize sources, given an argument. Each factory function
// is responsible to initialize sources of one registered type
typedef source_t* (*factory_fn_t)(const std::string&);

// Plugins are expected to have an 'initialize' function, which should
// return an array of structures of the following format
typedef struct {
    const char* type;
    factory_fn_t factory;
} source_info_t;

typedef const source_info_t* (*initialize_fn_t)(void);

}}

#endif
