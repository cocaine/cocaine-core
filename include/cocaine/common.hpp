/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/platform.hpp"

#if !defined(HAVE_CLANG) && !defined(HAVE_GCC46)
    #define nullptr __null
#endif

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED

#if !defined(COCAINE_DEBUG)
    #define BOOST_DISABLE_ASSERTS
#endif

#include <boost/assert.hpp>
#include <boost/version.hpp>

#define COCAINE_DECLARE_NONCOPYABLE(_name_)     \
    _name_(const _name_& other) = delete;       \
                                                \
    _name_&                                     \
    operator=(const _name_& other) = delete;

#if defined(HAVE_GCC47) || defined(TARGET_OS_MAC)
    #define COCAINE_HAVE_FEATURE_STEADY_CLOCK
    #define COCAINE_HAVE_FEATURE_UNDERLYING_TYPE
#endif

#include "cocaine/config.hpp"
#include "cocaine/exceptions.hpp"
#include "cocaine/forwards.hpp"

#endif
