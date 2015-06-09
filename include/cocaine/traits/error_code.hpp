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

#ifndef COCAINE_ERROR_CODE_SERIALIZATION_TRAITS_HPP
#define COCAINE_ERROR_CODE_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"
#include "cocaine/traits/tuple.hpp"

#include <system_error>

namespace cocaine { namespace io {

template<>
struct type_traits<std::error_code> {
    typedef boost::mpl::list<int, int>::type sequence_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const std::error_code& source) {
        int category_id = error::registrar::map(source.category());
        int ec          = source.value();

        type_traits<sequence_type>::pack(target, category_id, ec);
    }

    static inline
    void
    unpack(const msgpack::object& source, std::error_code& target) {
        int category_id;
        int ec;

        type_traits<sequence_type>::unpack(source, category_id, ec);

        target.assign(ec, error::registrar::map(category_id));
    }
};

}} // namespace cocaine::io

#endif
