#ifndef COCAINE_SECURITY_HPP
#define COCAINE_SECURITY_HPP

#include <openssl/evp.h>

#include "cocaine/common.hpp"

namespace cocaine { namespace security {

class signatures_t:
    public boost::noncopyable
{
    public:
        signatures_t();
        ~signatures_t();

        void verify(const char* message,
                    size_t message_size,
                    const unsigned char* signature,
                    size_t signature_size,
                    const std::string& username);

        // std::string sign(const std::string& message, const std::string& username);

    private:
        EVP_MD_CTX* m_context;

        typedef std::map<const std::string, EVP_PKEY*> key_map_t;
        key_map_t m_keys;
};

}}

#endif
