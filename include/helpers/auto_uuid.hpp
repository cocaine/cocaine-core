#ifndef YAPPI_UUID_HPP
#define YAPPI_UUID_HPP

#include <string>
#include <stdexcept>

#include <uuid/uuid.h>

namespace yappi { namespace helpers {

class auto_uuid_t {
    public:
        auto_uuid_t() {
            uuid_t uuid;
            char unparsed_uuid[37];

            uuid_generate(uuid);
            uuid_unparse(uuid, unparsed_uuid);

            m_uuid = unparsed_uuid;
        }

        explicit auto_uuid_t(const std::string& uuid):
            m_uuid(uuid)
        {
            uuid_t dummy;

            // Validate the given uuid
            if(uuid_parse(m_uuid.c_str(), dummy) == -1) {
                throw std::runtime_error("invalid uuid");
            }
        }

        inline std::string get() const {
            return m_uuid;
        }

    private:
        std::string m_uuid;
};

}}

#endif
