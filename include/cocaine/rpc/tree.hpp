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

#ifndef COCAINE_IO_DISPATCH_TREE_HPP
#define COCAINE_IO_DISPATCH_TREE_HPP

#include "cocaine/common.hpp"

namespace cocaine {

struct dispatch_tree_t:
    public std::map<int, std::tuple<std::string, dispatch_tree_t>>
{
    typedef std::map<
        int,
        std::tuple<std::string, dispatch_tree_t>
    > mapping_type;

    dispatch_tree_t(): mapping_type() { }
    dispatch_tree_t(const dispatch_tree_t& o): mapping_type(o) { }
    dispatch_tree_t(dispatch_tree_t&& o): mapping_type(std::move(o)) { }

    dispatch_tree_t& operator=(const dispatch_tree_t& lhs) {
        static_cast<mapping_type&>(*this) = lhs; return *this;
    }

    dispatch_tree_t& operator=(dispatch_tree_t&& lhs) {
        static_cast<mapping_type&>(*this) = std::move(lhs); return *this;
    }
};

} // namespace cocaine

#endif
