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

#ifndef COCAINE_COMMON_HPP
#define COCAINE_COMMON_HPP

#include <map>
#include <stdexcept>
#include <string>
#include <tr1/cstdint>
#include <vector>

#include <boost/assert.hpp>
#include <boost/make_shared.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION >= 104000
# include <boost/ptr_container/ptr_unordered_map.hpp>
#else
# include <boost/ptr_container/ptr_map.hpp>
#endif

#define EV_MINIMAL 0

#include <ev++.h>

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include "cocaine/helpers/birth_control.hpp"
#include "cocaine/helpers/json.hpp"
#include "cocaine/helpers/unique_id.hpp"

using cocaine::helpers::birth_control_t;
using cocaine::helpers::unique_id_t;

#endif
