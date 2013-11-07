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

#ifndef COCAINE_DYNAMIC_OBJECT_HPP
#define COCAINE_DYNAMIC_OBJECT_HPP

#include <map>

namespace cocaine {

class dynamic_t::object_t :
    public std::map<std::string, cocaine::dynamic_t>
{
    typedef std::map<std::string, cocaine::dynamic_t>
            base_type;
public:
    object_t() {
        // pass
    }

    template<class InputIt>
    object_t(InputIt first, InputIt last) :
        base_type(first, last)
    {
        // pass
    }

    object_t(const object_t& other) :
        base_type(other)
    {
        // pass
    }

    object_t(object_t&& other) :
        base_type(std::move(other))
    {
        // pass
    }

    object_t(std::initializer_list<value_type> init) :
        base_type(init)
    {
        // pass
    }

    object_t(const base_type& other) :
        base_type(other)
    {
        // pass
    }

    object_t(base_type&& other) :
        base_type(std::move(other))
    {
        // pass
    }

    object_t&
    operator=(const object_t& other) {
        base_type::operator=(other);
        return *this;
    }

    object_t&
    operator=(object_t&& other) {
        base_type::operator=(std::move(other));
        return *this;
    }

    using base_type::at;

    cocaine::dynamic_t&
    at(const std::string& key, cocaine::dynamic_t& def);

    const cocaine::dynamic_t&
    at(const std::string& key, const cocaine::dynamic_t& def) const;

    using base_type::operator[];

    const cocaine::dynamic_t&
    operator[](const std::string& key) const;
};

} // namespace cocaine

#endif // COCAINE_DYNAMIC_OBJECT_HPP
