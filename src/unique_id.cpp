/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/unique_id.hpp"

#include <uuid/uuid.h>

using namespace cocaine;

unique_id_t::unique_id_t() {
    uuid_generate(reinterpret_cast<unsigned char*>(uuid.data()));
}

std::string
unique_id_t::string() const {
    // A storage for a 36-character long UUID plus the trailing zero.
    char unparsed[37];

    uuid_unparse_lower(
        reinterpret_cast<const unsigned char*>(uuid.data()),
        unparsed
    );

    return unparsed;
}

bool
unique_id_t::operator==(const unique_id_t& other) const {
    return uuid[0] == other.uuid[0] &&
           uuid[1] == other.uuid[1];
}

namespace cocaine {

std::ostream&
operator<<(std::ostream& stream, const unique_id_t& id) {
    return stream << id.string();
}

} // namespace cocaine
