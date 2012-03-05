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

#ifdef HAVE_CGROUPS 
    #include <libcgroup.h>
#endif

#include "cocaine/context.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/auth.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/storages.hpp"

using namespace cocaine;
using namespace cocaine::storages;

context_t::context_t(config_t config_, std::auto_ptr<logging::sink_t> sink):
    config(config_),
    m_sink(sink)
{
    const int HOSTNAME_MAX_LENGTH = 256;
    char hostname[HOSTNAME_MAX_LENGTH];

    if(gethostname(hostname, HOSTNAME_MAX_LENGTH) == 0) {
        config.core.hostname = hostname;
    } else {
        throw std::runtime_error("failed to determine the hostname");
    }
   
#ifdef HAVE_CGROUPS 
    config.core.cgroups = (cgroup_init() == 0);
#else
    config.core.cgroups = false;
#endif

    // Builtins.
    registry().install<void_storage_t, storage_t>("void");
    registry().install<file_storage_t, storage_t>("files");
}

context_t::context_t(const context_t& other):
    config(other.config),
    m_sink(other.m_sink),
    m_registry(other.m_registry)
{ }

context_t& context_t::operator=(const context_t& other) {
    config = other.config;
    m_sink = other.m_sink;
    
    m_registry = other.m_registry;

    return *this;
}

logging::sink_t& context_t::sink() {
    if(!m_sink) {
        throw std::runtime_error("logging is not initialized");
    }

    return *m_sink;
}

core::registry_t& context_t::registry() {
    boost::lock_guard<boost::recursive_mutex> lock(m_mutex);
    
    if(!m_registry) {
        m_registry.reset(new core::registry_t(*this));
    }

    return *m_registry;
}

storage_t& context_t::storage() {
    boost::lock_guard<boost::recursive_mutex> lock(m_mutex);

    if(!m_storage) {
        m_storage = registry().create<storage_t>(config.storage.driver);
    }

    return *m_storage;
}

zmq::context_t& context_t::io() {
    boost::lock_guard<boost::recursive_mutex> lock(m_mutex);
    
    if(!m_io) {
        m_io.reset(new zmq::context_t(1));
    }

    return *m_io;
}

crypto::auth_t& context_t::auth() {
    boost::lock_guard<boost::recursive_mutex> lock(m_mutex);
    
    if(!m_auth) {
        m_auth.reset(new crypto::auth_t(*this));
    }

    return *m_auth;
}
