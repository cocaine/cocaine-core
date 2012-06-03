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

#include "cocaine/interfaces/plugin.hpp"

#include "cocaine/rpc.hpp"
#include "cocaine/slave.hpp"

using namespace cocaine;
using namespace cocaine::engine;

io_t::io_t(slave_t& slave):
    m_slave(slave)
{ }

blob_t io_t::read(int timeout) {
    return m_slave.recv(timeout);
}

void io_t::write(const void * data, size_t size) {
    rpc::packed<rpc::chunk> packed(data, size);
    m_slave.send(packed);
}

plugin_t::plugin_t(context_t& context, const manifest_t& manifest):
    m_context(context),
    m_manifest(manifest)
{ }

plugin_t::~plugin_t() { }
