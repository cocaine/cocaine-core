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

#ifndef COCAINE_MODULE_HPP
#define COCAINE_MODULE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace core {

// Module helper
// -------------

template<class T>
class module_t {
	public:
		static object_t* create(context_t& ctx) {
			return new T(ctx);
		}
};

// Module initialization
// ---------------------

typedef object_t* (*factory_fn_t)(context_t& context);
typedef void (*initialize_fn_t)(registry_t& registry);

}}

#endif