/*
    Copyright (c) 2017+ Anton Matveenko <antmat@me.com>
    Copyright (c) 2017+ Other contributors as noted in the AUTHORS file.

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

#include <cocaine/dynamic.hpp>

#include <memory>

namespace cocaine {

template<class T>
struct dynamic_constructor<std::shared_ptr<T>> {
    static const bool enable = true;

    static inline
    void
    convert(const std::shared_ptr<T>& from, dynamic_t::value_t& to) {
        if(from) {
            dynamic_constructor<typename std::remove_const<T>::type>::convert(*from, to);
        } else {
            to = dynamic_t::null_t();
        }
    }
};

} // namespace cocaine
