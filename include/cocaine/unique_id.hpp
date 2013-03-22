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

#include <boost/functional/hash.hpp>

#include <uuid/uuid.h>

namespace cocaine {

struct uninitialized_t { };

static
const uninitialized_t
uninitialized = uninitialized_t();

struct unique_id_t {
    unique_id_t() {
        uuid_generate(reinterpret_cast<unsigned char*>(uuid.data()));
    }

    explicit
    unique_id_t(uninitialized_t) {
        // NOTE: If this UUID is going to be a target for a message, there's no
        // need to generate it, which saves a couple of ticks.
    }

    explicit
    unique_id_t(const std::string& other) {
        int rv = uuid_parse(
            other.c_str(),
            reinterpret_cast<unsigned char*>(uuid.data())
        );

        if(rv != 0) {
            throw cocaine::error_t("unable to parse '%s' as an unique id", other);
        }
    }

    std::string
    string() const {
        // A storage for a 36-character long UUID plus the trailing zero.
        char unparsed[37];

        uuid_unparse_lower(
            reinterpret_cast<const unsigned char*>(uuid.data()),
            unparsed
        );

        return unparsed;
    }

    bool
    operator==(const unique_id_t& other) const {
        return uuid[0] == other.uuid[0] &&
               uuid[1] == other.uuid[1];
    }

    friend
    std::ostream&
    operator<<(std::ostream& stream,
               const unique_id_t& id)
    {
        return stream << id.string();
    }

public:
    // NOTE: Store 128-bit UUIDs as two 64-bit unsigned integers.
    std::array<uint64_t, 2> uuid;
};

static inline
size_t
hash_value(const unique_id_t& id) {
    return boost::hash_range(id.uuid.begin(), id.uuid.end());
}

} // namespace cocaine

#endif
