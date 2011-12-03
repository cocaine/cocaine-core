#ifndef COCAINE_UNIQUE_ID_HPP
#define COCAINE_UNIQUE_ID_HPP

#include <uuid/uuid.h>

namespace cocaine { namespace helpers {

class unique_id_t {
    public:
        typedef std::string type;

        unique_id_t() {
            uuid_generate(m_uuid);
        }

        explicit unique_id_t(const type& other) {
            uuid_t uuid;

            if(uuid_parse(other.c_str(), uuid) == 0) {
                m_id = other;
            } else {
                throw std::runtime_error("invalid unique id");
            }
        }

        inline const type& id() const {
            if(m_id.empty()) {
                char unparsed_uuid[37];
                uuid_unparse_lower(m_uuid, unparsed_uuid);
                m_id = unparsed_uuid;
            }

            return m_id;
        }

    private:
        uuid_t m_uuid;
        mutable type m_id;
};

}}

#endif
