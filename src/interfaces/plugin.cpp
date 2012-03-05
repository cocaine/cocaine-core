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

using namespace cocaine::engine;

io_t::io_t(overseer_t& overseer, const void* request_, size_t request_size_):
	request(request_),
	request_size(request_size_),
	m_overseer(overseer)
{ }

void io_t::pull() {
	// TODO: Streaming.
}

void io_t::push(const void* data, size_t size) {
	m_overseer.send(
		rpc::push,
		data,
		size
	);
}

void io_t::emit(const std::string& key, const void* data, size_t size) {
	// TODO: Emitters.
}
