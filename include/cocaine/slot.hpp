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

#ifndef COCAINE_REACTOR_SLOT_HPP
#define COCAINE_REACTOR_SLOT_HPP

#include "cocaine/common.hpp"
#include "cocaine/channel.hpp"

#include <boost/function.hpp>
#include <boost/function_types/function_type.hpp>

#include <boost/mpl/push_front.hpp>

namespace cocaine {

namespace ft = boost::function_types;
namespace mpl = boost::mpl;

template<class Sequence, typename R>
struct callable {
    typedef typename ft::function_type<
        typename mpl::push_front<Sequence, R>::type
    >::type function_type;

    typedef boost::function<function_type> type;
};

struct slot_base_t {
    virtual
    msgpack::object
    operator()(const msgpack::object& request) = 0;
};

template<class Event, typename R>
struct slot:
    public slot_base_t
{
    typedef typename io::event_traits<Event>::tuple_type tuple_type;
    typedef typename callable<tuple_type, R>::type callable_type;

    slot(callable_type callable):
        m_callable(callable)
    { }

    virtual
    msgpack::object
    operator()(const msgpack::object& tuple) {
        if(tuple.type != msgpack::type::ARRAY ||
           tuple.via.array.size != io::event_traits<Event>::length)
        {
            throw msgpack::type_error();
        }

        typedef typename mpl::begin<tuple_type>::type begin;
        typedef typename mpl::end<tuple_type>::type end;

        msgpack::object response;

        invoke<begin, end>::apply(
            m_callable,
            tuple.via.array.ptr
        );

        return response;
    }

private:
    template<class It, class End>
    struct invoke {
        template<typename... Args>
        static inline
        R
        apply(callable_type& callable,
              msgpack::object * tuple,
              Args&&... args)
        {
            typedef typename mpl::deref<It>::type argument_type;
            typedef typename mpl::next<It>::type next_type;
            
            argument_type argument;

            io::type_traits<argument_type>::unpack(*tuple, argument);

            return invoke<next_type, End>::apply(
                callable,
                ++tuple,
                std::forward<Args>(args)...,
                std::move(argument)
            );
        }
    };

    template<class End>
    struct invoke<End, End> {
        template<typename... Args>
        static inline
        R
        apply(callable_type& callable,
              msgpack::object * tuple,
              Args&&... args)
        {
            return callable(std::forward<Args>(args)...);
        }
    };

private:
    callable_type m_callable;
};

} // namespace cocaine

#endif
