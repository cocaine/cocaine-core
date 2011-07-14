#ifndef YAPPI_UUID_HPP
#define YAPPI_UUID_HPP

#include <string>
#include <uuid/uuid.h>

namespace yappi { namespace helpers {

class id_t {
    public:
        id_t() {
            uuid_t uuid;
            char unparsed_uuid[37];

            uuid_generate(uuid);
            uuid_unparse(uuid, unparsed_uuid);

            m_uuid = unparsed_uuid;
        }

        inline std::string get() const {
            return m_uuid;
        }

    private:
        std::string m_uuid;
};

}}

#endif
