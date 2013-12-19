/*
    Copyright (c) 2013-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_IO_VARIANT_SERIALIZATION_TRAITS_HPP
#define COCAINE_IO_VARIANT_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

#include <boost/variant.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/size.hpp>

namespace cocaine { namespace io {

namespace detail { namespace variant_traits {

template<class Stream>
struct packer :
    public boost::static_visitor<>
{
    packer(msgpack::packer<Stream>& target):
        m_target(target)
    { }

    template<class T>
    void
    operator()(const T& value) const {
        type_traits<T>::pack(m_target, value);
    }

private:
    msgpack::packer<Stream>& m_target;
};

template<class Variant, int N, class = void>
struct unpacker {
    static inline
    void
    unpack(int which, const msgpack::object& source, Variant& target) {
        if(which == N) {
            typedef typename boost::mpl::at<typename Variant::types, boost::mpl::int_<N>>::type
                    result_type;

            result_type result;
            type_traits<result_type>::unpack(source, result);
            target = result;
        } else {
            unpacker<Variant, N + 1>::unpack(which, source, target);
        }
    }
};

template<class Variant, int N>
struct unpacker<
    Variant,
    N,
    typename std::enable_if<N == boost::mpl::size<typename Variant::types>::type::value>::type
> {
    static inline
    void
    unpack(int which, const msgpack::object& source, Variant& target) {
        throw std::bad_cast();
    }
};

}} // namespace detail::variant_traits

template<BOOST_VARIANT_ENUM_PARAMS(typename T)>
struct type_traits<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>> {
    typedef boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>
            variant_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const variant_type& source) {
        target.pack_array(2);
        target << source.which();
        boost::apply_visitor(detail::variant_traits::packer<Stream>(target), source);
    }

    static inline
    void
    unpack(const msgpack::object& source, variant_type& target) {
        if(source.type != msgpack::type::ARRAY ||
           source.via.array.size != 2 ||
           source.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER)
        {
            throw std::bad_cast();
        }

        detail::variant_traits::unpacker<variant_type, 0>::unpack(source.via.array.ptr[0].via.u64,
                                                                  source.via.array.ptr[1],
                                                                  target);
    }
};

}} // namespace cocaine::io

#endif
