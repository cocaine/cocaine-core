#ifndef YAPPI_SECURITY_HPP
#define YAPPI_SECURITY_HPP

#include <openssl/evp.h>

#include "common.hpp"

namespace yappi { namespace security {

namespace fs = boost::filesystem;

class authorizer_t: public boost::noncopyable {
    public:
        authorizer_t(const std::string& uuid);
        ~authorizer_t();

        void verify(const std::string& message, const unsigned char* signature,
            size_t size, const std::string& token);

    private:
        EVP_MD_CTX* m_context;

        typedef std::map<const std::string, EVP_PKEY*> key_map_t;
        key_map_t m_keys;
};

}}

#endif
