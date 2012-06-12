/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#ifndef COCAINE_COMMON_HPP
#define COCAINE_COMMON_HPP

#include <map>
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

#define EV_MINIMAL       0
#define EV_USE_MONOTONIC 1
#define EV_USE_REALTIME  1
#define EV_USE_NANOSLEEP 1
#define EV_USE_EVENTFD   1

#include "cocaine/forwards.hpp"
#include "cocaine/exceptions.hpp"

#endif
