/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_PLATFORM_HPP
#define COCAINE_PLATFORM_HPP

#if defined(__GNUC__)
    #if (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) || __GNUC__ >= 5
        #define HAVE_GCC44
    #endif

    #if (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ >= 5
        #define HAVE_GCC46
    #endif

    #if (__GNUC__ == 4 && __GNUC_MINOR__ >= 8) || __GNUC__ >= 5
        #define HAVE_GCC48
    #endif
#endif

#if defined(__clang__) || defined(HAVE_GCC47)
    #define COCAINE_HAS_FEATURE_UNDERLYING_TYPE
#endif

#if defined(__clang__) || defined(HAVE_GCC48)
    #define COCAINE_HAS_FEATURE_STEADY_CLOCK
    #define COCAINE_HAS_FEATURE_PAIR_TO_TUPLE_CONVERSION
#endif

#endif
