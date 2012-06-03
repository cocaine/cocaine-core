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

#include "cocaine/overseer.hpp"
#include "cocaine/rpc.hpp"

using namespace cocaine;
using namespace cocaine::engine;

io_t::io_t(overseer_t& overseer):
    m_overseer(overseer)
{ }

blob_t io_t::read(int timeout) {
    return m_overseer.recv(timeout);
}

void io_t::write(const void * data, size_t size) {
    rpc::packed<rpc::chunk> packed(data, size);
    m_overseer.send(packed);
}

plugin_t::plugin_t(context_t& context, const manifest_t& manifest):
    m_context(context),
    m_manifest(manifest)
{ }

plugin_t::~plugin_t() { }
