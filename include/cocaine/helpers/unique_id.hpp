#ifndef COCAINE_AUTO_UUID_HPP
#define COCAINE_AUTO_UUID_HPP

#include <uuid/uuid.h>

namespace cocaine { namespace helpers {

class unique_id_t {
    public:
        typedef std::string type;

        unique_id_t() {
            uuid_t uuid;
            char unparsed_uuid[37];

            uuid_generate(uuid);
            uuid_unparse_lower(uuid, unparsed_uuid);

            m_id = unparsed_uuid;
        }

        explicit unique_id_t(const type& other) {
            uuid_t uuid;

            if(uuid_parse(other.c_str(), uuid) == 0) {
                m_id = other;
            } else {
                throw std::runtime_error("invalid unique id");
            }
        }

        inline std::string id() const {
            return m_id;
        }

    private:
        type m_id;
};

}}

#endif
