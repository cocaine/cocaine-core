/*
    Copyright (c) 2016+ Anton Matveenko <antmat@me.com>
    Copyright (c) 2016+ Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_CONTEXT_FILTER_HPP
#define COCAINE_CONTEXT_FILTER_HPP

#include <blackhole/attribute.hpp>
#include <blackhole/attributes.hpp>
#include <blackhole/message.hpp>
#include <blackhole/severity.hpp>

namespace cocaine {

/**
 * It is a wrapper on std::function
 * We use this, because class unlike typedef can be forwarded
 **/
class filter_t {
public:
    typedef blackhole::severity_t severity_t;
    typedef blackhole::attribute_pack attribute_pack;

    typedef std::function<bool(severity_t, attribute_pack& pack)> inner_t;

    filter_t(inner_t _inner) : inner(std::move(_inner)) {}

    bool operator()(severity_t severity, attribute_pack& pack) const {
        return inner(severity, pack);
    }
private:
    inner_t inner;
};

}
#endif
