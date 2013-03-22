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

#if !defined(__clang__) && (defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6))
    #define nullptr __null
#endif

#if defined(__clang__)
    #define unused [[gnu::unused]]
#else
    #define unused __attribute__((unused))
#endif

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#define BOOST_FILESYSTEM_VERSION 3

#ifndef COCAINE_DEBUG
    #define BOOST_DISABLE_ASSERTS
#endif

#include <boost/assert.hpp>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>
#include <boost/version.hpp>

#include "cocaine/config.hpp"
#include "cocaine/exceptions.hpp"
#include "cocaine/forwards.hpp"

#endif
