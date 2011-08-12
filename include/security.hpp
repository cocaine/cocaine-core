#ifndef YAPPI_SECURITY_HPP
#define YAPPI_SECURITY_HPP

#include <openssl/evp.h>

#include "common.hpp"

namespace yappi { namespace security {

class signing_t: public boost::noncopyable {
    public:
        signing_t(const std::string& uuid);
        ~signing_t();

        std::string sign(const std::string& message, const std::string& token);
        
        void verify(const std::string& message, const unsigned char* signature,
                    unsigned int size, const std::string& token);

    private:
        EVP_MD_CTX* m_context;

        typedef std::map<const std::string, EVP_PKEY*> key_map_t;
        key_map_t m_public_keys, m_private_keys;
};

}}

#endif
