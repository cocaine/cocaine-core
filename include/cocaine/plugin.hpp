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

#include "cocaine/common.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/manifest.hpp"

namespace cocaine { namespace plugin {

// Exceptions
// ----------

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

// Invocation context
// ------------------

class invocation_context_t {
    public:
        void push(const void* data, size_t size);
        void emit(const std::string& key, const void* data, size_t size);

    public:
        void* request;
        size_t request_size;
};

class module_t:
    public boost::noncopyable
{
    public:
        module_t(context_t& context, engine::manifest_t& manifest):
            m_context(context),
            m_manifest(manifest),
            m_log(context, "engine " + manifest.name)
        {
            m_log.debug("%s module constructing", m_manifest.type.c_str());
        }

        virtual ~module_t() {
            m_log.debug("%s module destructing", m_manifest.type.c_str());
        }

        virtual void invoke(invocation_context_t& context) = 0;

    private:
        context_t& m_context;
        engine::manifest_t& m_manifest;
        logging::emitter_t m_log;
};

// Plugin initialization
// ---------------------

typedef module_t* (*factory_fn_t)(context_t& context, engine::manifest_t& manifest);

typedef struct {
    const char* type;
    factory_fn_t factory;
} module_info_t;

typedef const module_info_t* (*initialize_fn_t)(void);

}}

#endif
