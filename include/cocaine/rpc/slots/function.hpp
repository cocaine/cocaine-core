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

#ifndef COCAINE_IO_FUNCTION_SLOT_HPP
#define COCAINE_IO_FUNCTION_SLOT_HPP

#include "cocaine/common.hpp"

#include "cocaine/idl/streaming.hpp"

#include "cocaine/rpc/protocol.hpp"
#include "cocaine/rpc/slot.hpp"
#include "cocaine/rpc/tags.hpp"

#include "cocaine/traits.hpp"
#include "cocaine/traits/enum.hpp"

#include <functional>

#include <boost/function_types/function_type.hpp>

#include <boost/mpl/count_if.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/transform.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

namespace aux {

template<class T>
struct unpack_impl {
    template<class ArgumentIterator>
    static inline
    ArgumentIterator
    apply(ArgumentIterator it, ArgumentIterator end, T& argument) {
        BOOST_VERIFY(it != end);

        try {
            type_traits<T>::unpack(*it++, argument);
        } catch(const msgpack::type_error& e) {
            throw cocaine::error_t("argument type mismatch");
        }

        return it;
    }
};

template<class T>
struct unpack_impl<optional<T>> {
    template<class ArgumentIterator>
    static inline
    ArgumentIterator
    apply(ArgumentIterator it, ArgumentIterator end, T& argument) {
        if(it != end) {
            return unpack_impl<T>::apply(it, end, argument);
        } else {
            argument = T();
        }

        return it;
    }
};

template<class T, T Default>
struct unpack_impl<optional_with_default<T, Default>> {
    typedef T type;

    template<class ArgumentIterator>
    static inline
    ArgumentIterator
    apply(ArgumentIterator it, ArgumentIterator end, T& argument) {
        if(it != end) {
            return unpack_impl<T>::apply(it, end, argument);
        } else {
            argument = Default;
        }

        return it;
    }
};

template<class It, class End>
struct invoke_impl {
    template<class F, class ArgumentIterator, typename... Args>
    static inline
    typename result_of<F>::type
    apply(const F& callable, ArgumentIterator it, ArgumentIterator end, Args&&... args) {
        typedef typename mpl::deref<It>::type argument_type;
        typename detail::unwrap_type<argument_type>::type argument;

        it = unpack_impl<argument_type>::apply(it, end, argument);

        return invoke_impl<typename mpl::next<It>::type, End>::apply(
            callable,
            it,
            end,
            std::forward<Args>(args)...,
            std::move(argument)
        );
    }
};

template<class End>
struct invoke_impl<End, End> {
    template<class F, class ArgumentIterator, typename... Args>
    static inline
    typename result_of<F>::type
    apply(const F& callable, ArgumentIterator, ArgumentIterator, Args&&... args) {
        return callable(std::forward<Args>(args)...);
    }
};

} // namespace aux

#if defined(__GNUC__) && !defined(HAVE_GCC46)
    #pragma GCC diagnostic ignored "-Wtype-limits"
#endif

template<class Sequence>
struct invoke {
    template<class F>
    static inline
    typename result_of<F>::type
    apply(const F& callable, const msgpack::object& unpacked) {
        const size_t required = mpl::count_if<
            Sequence,
            mpl::lambda<detail::is_required<mpl::arg<1>>>
        >::value;

        if(unpacked.type != msgpack::type::ARRAY) {
            throw cocaine::error_t("argument sequence type mismatch");
        }

        #if defined(__clang__)
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wtautological-compare"
        #elif defined(__GNUC__) && defined(HAVE_GCC46)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wtype-limits"
        #endif

        // NOTE: In cases when the callable is nullary or every parameter is optional, this
        // comparison becomes tautological and emits dead code (unsigned < 0).
        // This is a known compiler bug: http://llvm.org/bugs/show_bug.cgi?id=8682
        if(unpacked.via.array.size < required) {
            throw cocaine::error_t(
                "argument sequence length mismatch - expected at least %d, got %d",
                required,
                unpacked.via.array.size
            );
        }

        #if defined(__clang__)
            #pragma clang diagnostic pop
        #elif defined(__GNUC__) && defined(HAVE_GCC46)
            #pragma GCC diagnostic pop
        #endif

        typedef typename mpl::begin<Sequence>::type begin;
        typedef typename mpl::end<Sequence>::type end;

        const std::vector<msgpack::object> args(
            unpacked.via.array.ptr,
            unpacked.via.array.ptr + unpacked.via.array.size
        );

        return aux::invoke_impl<begin, end>::apply(callable, args.begin(), args.end());
    }
};

template<class R, class Event>
struct function_slot:
    public basic_slot<Event>
{
    typedef typename event_traits<Event>::tuple_type tuple_type;
    typedef typename event_traits<Event>::result_type upstream_type;

    typedef typename mpl::transform<
        tuple_type,
        mpl::lambda<detail::unwrap_type<mpl::arg<1>>>
    >::type sequence_type;

    typedef typename boost::function_types::function_type<
        typename mpl::push_front<sequence_type, R>::type
    >::type function_type;

    typedef std::function<function_type> callable_type;

    function_slot(callable_type callable_):
        callable(callable_)
    { }

    R
    call(const msgpack::object& unpacked) const {
        return invoke<tuple_type>::apply(callable, unpacked);
    }

private:
    const callable_type callable;
};

}} // namespace cocaine::io

#endif
