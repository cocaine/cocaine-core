/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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


#pragma once

#include <boost/config.hpp>

#include <boost/mpl/contains.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/vector.hpp>

#include <boost/mpl/vector/vector50.hpp>
#include <boost/mpl/aux_/config/ctps.hpp>
#include <boost/preprocessor/iterate.hpp>

// Some woodoo magic of boost mpl.
// This allows to generate mpl vector larger than 50
// See http://comments.gmane.org/gmane.comp.lib.boost.user/6986

namespace boost { namespace mpl {

#define BOOST_PP_ITERATION_PARAMS_1 \
    (3,(51, 100, "boost/mpl/vector/aux_/numbered.hpp"))
#include BOOST_PP_ITERATE()

}} // namespace boost::mpl
