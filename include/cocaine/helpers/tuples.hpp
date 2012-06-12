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

#ifndef COCAINE_HELPERS_TUPLES_HPP
#define COCAINE_HELPERS_TUPLES_HPP

#include <boost/tuple/tuple.hpp>

namespace cocaine { namespace helpers {

using namespace boost::tuples;

template<class Current, class Next>
struct chain {
    typedef chain<typename Current::tail_type, Next> chain_type;
    typedef cons<typename Current::head_type, typename chain_type::type> type;

    static type apply(const Current& current,
                      const Next& next)
    {
        return type(current.get_head(), chain_type::apply(current.get_tail(), next));
    }
};

template<class Next>
struct chain<null_type, Next> {
    typedef chain<typename Next::tail_type, null_type> chain_type;
    typedef cons<typename Next::head_type, typename chain_type::type> type;

    static type apply(null_type null,
                      const Next& next)
    {
        return type(next.get_head(), chain_type::apply(next.get_tail(), null));
    }
};

template<>
struct chain<null_type, null_type> {
    typedef null_type type;
    static null_type null;

    static type apply(null_type,
                      null_type)
    {
        return null;
    }
};

}

template<class LT, class RT>
static inline typename helpers::chain<LT, RT>::type
join_tuples(const LT& lt,
            const RT& rt)
{
    return helpers::chain<LT, RT>::apply(lt, rt);
}

}

#endif
