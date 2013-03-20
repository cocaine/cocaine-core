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
    struct invoke {
        template<class F, typename... Args>
        static inline
        typename result_of<F>::type
        apply(const F& callable,
              const msgpack::object * packed,
              Args&&... args)
        {
            typedef typename mpl::deref<It>::type argument_type;
            typedef typename mpl::next<It>::type next_type;

            argument_type argument;

            try {
                io::type_traits<argument_type>::unpack(*packed, argument);
            } catch(const msgpack::type_error& e) {
                throw cocaine::error_t("argument type mismatch");
            } catch(const std::bad_cast& e) {
                throw cocaine::error_t("argument type mismatch");
            }

            return invoke<next_type, End>::apply(
                callable,
                ++packed,
                std::forward<Args>(args)...,
                std::move(argument)
            );
        }
    };

    template<class End>
    struct invoke<End, End> {
        template<class F, typename... Args>
        static inline
        typename result_of<F>::type
        apply(const F& callable,
              const msgpack::object * /* packed */,
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

struct slot_base_t {
    virtual
    std::string
    operator()(const msgpack::object& packed) = 0;
};

template<typename R, class Sequence>
struct slot:
    public slot_base_t
{
    typedef typename detail::callable<R, Sequence>::type callable_type;

    slot(callable_type callable):
        m_callable(callable)
    { }

    virtual
    std::string
    operator()(const msgpack::object& packed) {
        typedef typename mpl::begin<Sequence>::type begin;
        typedef typename mpl::end<Sequence>::type end;

        if(packed.type != msgpack::type::ARRAY ||
           packed.via.array.size != mpl::size<Sequence>::value)
        {
            throw cocaine::error_t("argument sequence mismatch");
        }

        const R result = detail::invoke<begin, end>::apply(
            m_callable,
            packed.via.array.ptr
        );

        std::ostringstream buffer;
        msgpack::packer<std::ostringstream> packer(buffer);

        io::type_traits<R>::pack(packer, result);

        return buffer.str();
    }

private:
    const callable_type m_callable;
};

} // namespace cocaine

#endif
