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

#include <stdexcept>
#include <string>

#include <uuid/uuid.h>

namespace cocaine { namespace helpers {

class unique_id_t {
    public:
        typedef std::string identifier_type;

        unique_id_t() {
            uuid_generate(m_uuid);
        }

        explicit unique_id_t(const identifier_type& other) {
            if(uuid_parse(other.c_str(), m_uuid) == 0) {
                m_id = other;
            } else {
                throw std::runtime_error("invalid unique id");
            }
        }

        const identifier_type& id() const {
            if(m_id.empty()) {
                char unparsed_uuid[37];
                uuid_unparse_lower(m_uuid, unparsed_uuid);
                m_id = unparsed_uuid;
            }

            return m_id;
        }

    private:
        uuid_t m_uuid;
        mutable identifier_type m_id;
};

} // namespace helpers

using helpers::unique_id_t;

} // namespace cocaine

#endif
