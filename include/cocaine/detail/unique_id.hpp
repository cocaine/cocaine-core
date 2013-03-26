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

#ifndef COCAINE_UNIQUE_ID_HPP
#define COCAINE_UNIQUE_ID_HPP

#include "cocaine/common.hpp"

#include <array>

namespace cocaine {

struct uninitialized_t { };

extern const uninitialized_t uninitialized;

struct unique_id_t {
    unique_id_t();

    explicit
    unique_id_t(uninitialized_t);

    explicit
    unique_id_t(const std::string& other);

    std::string
    string() const;

    bool
    operator==(const unique_id_t& other) const;

    friend
    std::ostream&
    operator<<(std::ostream& stream, const unique_id_t& id);

public:
    // NOTE: Store 128-bit UUIDs as two 64-bit unsigned integers.
    std::array<uint64_t, 2> uuid;
};

size_t
hash_value(const unique_id_t& id);

} // namespace cocaine

#endif
