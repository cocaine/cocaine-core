//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
            uuid_t uuid;

            if(uuid_parse(other.c_str(), uuid) == 0) {
                m_id = other;
            } else {
                throw std::runtime_error("invalid unique id");
            }
        }

        inline const identifier_type& id() const {
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

}

using helpers::unique_id_t;

}

#endif
