#ifndef COCAINE_UNIQUE_ID_HPP
#define COCAINE_UNIQUE_ID_HPP

#include <uuid/uuid.h>

namespace cocaine { namespace helpers {

class unique_id_t {
    public:
        typedef std::string type;
        typedef const type& reference;

        unique_id_t() {
            uuid_t uuid;
            char unparsed_uuid[37];

            uuid_generate(uuid);
            uuid_unparse_lower(uuid, unparsed_uuid);

            m_id = unparsed_uuid;
        }

        explicit unique_id_t(reference other) {
            uuid_t uuid;

            if(uuid_parse(other.c_str(), uuid) == 0) {
                m_id = other;
            } else {
                throw std::runtime_error("invalid unique id");
            }
        }

        inline reference id() const {
            return m_id;
        }

    private:
        type m_id;
};

}}

#endif
