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

#ifndef COCAINE_ACTOR_SLOT_HPP
#define COCAINE_ACTOR_SLOT_HPP

#include "cocaine/common.hpp"
#include "cocaine/traits.hpp"

#include "cocaine/api/stream.hpp"

#include <functional>
#include <sstream>

#include <boost/function_types/function_type.hpp>
#include <boost/mpl/push_front.hpp>

namespace cocaine {

namespace ft = boost::function_types;
namespace mpl = boost::mpl;

namespace detail {
    template<class T>
    struct depend {
        typedef void type;
    };

    template<class F, class = void>
    struct result_of {
        typedef typename std::result_of<F>::type type;
    };

    template<class F>
    struct result_of<
        F,
        typename depend<typename F::result_type>::type
    >
    {
        typedef typename F::result_type type;
    };

    template<class It, class End>
    struct invoke_impl {
        template<class F, typename... Args>
        static inline
        typename result_of<F>::type
        apply(const F& callable,
              const msgpack::object * ptr,
              Args&&... args)
        {
            typedef typename mpl::deref<It>::type argument_type;
            typedef typename mpl::next<It>::type next;

            argument_type argument;

            try {
                io::type_traits<argument_type>::unpack(*ptr, argument);
            } catch(const msgpack::type_error& e) {
                throw cocaine::error_t("argument type mismatch");
            }

            return invoke_impl<next, End>::apply(
                callable,
                ++ptr,
                std::forward<Args>(args)...,
                std::move(argument)
            );
        }
    };

    template<class End>
    struct invoke_impl<End, End> {
        template<class F, typename... Args>
        static inline
        typename result_of<F>::type
        apply(const F& callable,
              const msgpack::object * /* ptr */,
              Args&&... args)
        {
            return callable(std::forward<Args>(args)...);
        }
    };

    template<typename R, class Sequence>
    struct callable {
        typedef typename ft::function_type<
            typename mpl::push_front<Sequence, R>::type
        >::type function_type;

        typedef std::function<function_type> type;
    };
}

// Slot invocation mechanics

template<class Sequence>
struct invoke {
    template<class F>
    static inline
    typename detail::result_of<F>::type
    apply(const F& callable,
          const msgpack::object& args)
    {
        typedef typename mpl::begin<Sequence>::type begin;
        typedef typename mpl::end<Sequence>::type end;

        if(args.type != msgpack::type::ARRAY ||
           args.via.array.size != mpl::size<Sequence>::value)
        {
            throw cocaine::error_t("argument sequence mismatch");
        }

        return detail::invoke_impl<begin, end>::apply(
            callable,
            args.via.array.ptr
        );
    }
};

// Slot basics

struct slot_concept_t {
    virtual
    void
    operator()(const std::shared_ptr<api::stream_t>& upstream,
               const msgpack::object& args) = 0;
};

template<class R, class Sequence>
struct basic_slot:
    public slot_concept_t
{
    typedef typename detail::callable<
        R,
        Sequence
    >::type callable_type;

    basic_slot(callable_type callable):
        m_callable(callable)
    { }

protected:
    const callable_type m_callable;
};

// Synchronous slot

template<class R, class Sequence>
struct synchronous_slot:
    public basic_slot<R, Sequence>
{
    typedef basic_slot<R, Sequence> base_type;
    typedef typename base_type::callable_type callable_type;

    synchronous_slot(callable_type callable):
        base_type(callable)
    { }

    virtual
    void
    operator()(const std::shared_ptr<api::stream_t>& upstream,
               const msgpack::object& args)
    {
        msgpack::packer<api::stream_t> packer(*upstream);

        io::type_traits<R>::pack(packer, invoke<Sequence>::apply(
            this->m_callable,
            args
        ));

        upstream->close();
    }
};

// Synchronous slot specialization for void functions

template<class Sequence>
struct synchronous_slot<void, Sequence>:
    public basic_slot<void, Sequence>
{
    typedef basic_slot<void, Sequence> base_type;
    typedef typename base_type::callable_type callable_type;

    synchronous_slot(callable_type callable):
        base_type(callable)
    { }

    virtual
    void
    operator()(const std::shared_ptr<api::stream_t>& upstream,
               const msgpack::object& args)
    {
        invoke<Sequence>::apply(
            this->m_callable,
            args
        );

        upstream->close();
    }
};

// Asynchronous slot

template<class R, class Sequence>
struct asynchronous_slot:
    public basic_slot<R, Sequence>
{

};

} // namespace cocaine

#endif
