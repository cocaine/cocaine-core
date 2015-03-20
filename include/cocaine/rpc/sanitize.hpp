/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_IO_SANITIZE_HPP
#define COCAINE_IO_SANITIZE_HPP

#include "cocaine/rpc/tags.hpp"

#include <boost/mpl/back_inserter.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/joint_view.hpp>
#include <boost/mpl/lambda.hpp>
#include <boost/mpl/stable_partition.hpp>
#include <boost/mpl/vector.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

template<class Sequence>
struct sanitize {
    typedef typename mpl::stable_partition<
        Sequence,
        typename mpl::lambda<
            details::is_required<mpl::_1>
        >::type,
        mpl::back_inserter<mpl::vector<>>,
        mpl::back_inserter<mpl::vector<>>
    >::type partitions;

    static const bool value = mpl::equal<
        typename mpl::joint_view<typename partitions::first, typename partitions::second>::type,
        Sequence
    >::value;
};

}} // namespace cocaine::io

#endif
