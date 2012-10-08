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

#ifndef COCAINE_HELPERS_UNIQUE_ID_HPP
#define COCAINE_HELPERS_UNIQUE_ID_HPP

#include <boost/format.hpp>
#include <stdexcept>
#include <string>
#include <uuid/uuid.h>

namespace cocaine { namespace helpers {

struct unique_id_t {
    unique_id_t() {
        uuid_generate(
            reinterpret_cast<unsigned char*>(uuid)
        );
    }

    explicit
    unique_id_t(const std::string& other) {
        int rv = uuid_parse(
            other.c_str(),
            reinterpret_cast<unsigned char*>(uuid)
        );
        
        if(rv != 0) {
            boost::format message("unable to parse '%s' as an unique id");
            throw std::runtime_error((message % other).str());
        }
    }

    const std::string&
    string() const {
        if(cache.empty()) {
            // NOTE: 36-character long UUID plus trailing zero.
            char unparsed[37];

            uuid_unparse_lower(
                reinterpret_cast<const unsigned char*>(uuid),
                unparsed
            );

            cache = unparsed;
        }
    
        return cache;
    }

    bool
    operator==(const unique_id_t& other) const {
        return uuid[0] == other.uuid[0] &&
               uuid[1] == other.uuid[1];
    }

private:
    uint64_t uuid[2];

    // Textual representation cache.
    mutable std::string cache;
};

} // namespace helpers

using helpers::unique_id_t;

} // namespace cocaine

#endif
