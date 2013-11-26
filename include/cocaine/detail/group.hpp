/*
    Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_LOCATOR_GROUP_HPP
#define COCAINE_LOCATOR_GROUP_HPP

#include "cocaine/common.hpp"
#include "cocaine/detail/cached.hpp"

namespace cocaine {

struct group_t:
    cached<std::map<std::string, unsigned int>>
{
    group_t(context_t& context, const std::string& name);

    // Name of the routing group.
    const std::string name;
};

} // namespace cocaine

#endif
