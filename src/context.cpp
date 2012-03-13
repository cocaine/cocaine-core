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

#ifdef HAVE_CGROUPS 
    #include <libcgroup.h>
#endif

#include "cocaine/context.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/networking.hpp"
#include "cocaine/storages/void.hpp"
#include "cocaine/storages/files.hpp"

using namespace cocaine;
using namespace cocaine::storages;

config_t::config_t() {
    defaults.heartbeat_timeout = 30.0f;
    defaults.suicide_timeout = 600.0f;
    defaults.pool_limit = 10;
    defaults.queue_limit = 10;
}

context_t::context_t(config_t config_, std::auto_ptr<logging::sink_t> sink):
    config(config_),
    m_sink(sink)
{
    initialize();
}

context_t::context_t(config_t config_):
    config(config_),
    m_sink(new logging::void_sink_t())
{
    initialize();
}

void context_t::initialize() {
    const int HOSTNAME_MAX_LENGTH = 256;
    char hostname[HOSTNAME_MAX_LENGTH];

    if(gethostname(hostname, HOSTNAME_MAX_LENGTH) == 0) {
        config.runtime.hostname = hostname;
    } else {
        throw std::runtime_error("failed to determine the hostname");
    }
   
#ifdef HAVE_CGROUPS 
    config.runtime.cgroups = (cgroup_init() == 0);
#else
    config.runtime.cgroups = false;
#endif

    // Initialize the module registry.
    m_registry.reset(new core::registry_t(*this));

    // Register the builtins.
    m_registry->install<void_storage_t, storage_t>("void");
    m_registry->install<file_storage_t, storage_t>("files");
}

// XXX: Appears to be thread-unsafe in some hypothetical situations.
context_t::context_t(const context_t& other):
    config(other.config),
    m_sink(other.m_sink),
    m_registry(other.m_registry)
{ }

// XXX: Appears to be thread-unsafe in some hypothetical situations.
context_t& context_t::operator=(const context_t& other) {
    config = other.config;

    m_sink = other.m_sink;
    m_registry = other.m_registry;

    return *this;
}

boost::shared_ptr<logging::logger_t> context_t::log(const std::string& name) {
    return m_sink->get(name);
}

zmq::context_t& context_t::io() {
    {
        boost::lock_guard<boost::mutex> lock(m_mutex);
        
        if(!m_io) {
            m_io.reset(new zmq::context_t(1));
        }
    }

    return *m_io;
}

storage_t& context_t::storage() {
    {
        boost::lock_guard<boost::mutex> lock(m_mutex);

        if(!m_storage) {
            m_storage = create<storage_t>(config.storage.driver);
        }
    }

    return *m_storage;
}
