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

#include "cocaine/context.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/auth.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/storages.hpp"

using namespace cocaine;

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
}

context_t::context_t(const context_t& other):
    config(other.config),
    m_auth(other.m_auth),
    m_registry(other.m_registry),
    m_sink(other.m_sink)
{ }

context_t& context_t::operator=(const context_t& other) {
    config = other.config;
    m_auth = other.m_auth;
    m_registry = other.m_registry;
    m_sink = other.m_sink;

    return *this;
}

crypto::auth_t& context_t::auth() {
    if(!m_auth) {
        m_auth.reset(new crypto::auth_t(*this));
    }

    return *m_auth;
}

zmq::context_t& context_t::io() {
    if(!m_io) {
        m_io.reset(new zmq::context_t(1));
    }

    return *m_io;
}

core::registry_t& context_t::registry() {
    if(!m_registry) {
        m_registry.reset(new core::registry_t(*this));
    }

    return *m_registry;
}

logging::sink_t& context_t::sink() {
    if(!m_sink) {
        throw std::runtime_error("logging is not initialized");
    }

    return *m_sink;
}

storages::storage_t& context_t::storage() {
    std::string driver(config.storage.driver);

    if(driver == "files") {
        m_storage.reset(new storages::file_storage_t(*this));
    } else if(driver == "mongo") {
        m_storage.reset(new storages::mongo_storage_t(*this));
    } else {
        m_storage.reset(new storages::void_storage_t(*this));
    }

    return *m_storage;
}
