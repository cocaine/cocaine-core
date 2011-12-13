#ifndef COCAINE_IDENTIFIABLE_HPP
#define COCAINE_IDENTIFIABLE_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace helpers {

class identifiable_t {
    public:
        identifiable_t(const std::string& identity):
            m_identity(identity)
        { }

    public:
        const char* identity() const {
            return m_identity.c_str();
        }

    private:
        std::string m_identity;
};

}}

#endif
