//
// Copyright (C) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
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

#ifndef _COCAINE_DEALER_GLOBALS_HPP_INCLUDED_
#define _COCAINE_DEALER_GLOBALS_HPP_INCLUDED_

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/core/host_info.hpp"
#include "cocaine/dealer/core/service.hpp"
#include "cocaine/dealer/core/service_info.hpp"
#include "cocaine/dealer/core/handle.hpp"
#include "cocaine/dealer/core/handle_info.hpp"

namespace cocaine {
namespace dealer {

// host info
typedef host_info<LT> host_info_t;

// service info
typedef service_info<LT> service_info_t;
typedef service<LT> service_t;

// handle info
typedef handle_info<LT> handle_info_t;
typedef handle<LT> handle_t;

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_GLOBALS_HPP_INCLUDED_
