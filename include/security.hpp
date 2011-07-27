#ifndef YAPPI_SECURITY_HPP
#define YAPPI_SECURITY_HPP

#include <openssl/evp.h>

#include "common.hpp"

namespace yappi { namespace security {

namespace fs = boost::filesystem;

class auth_t: public boost::noncopyable {
    public:
        auth_t(const std::string& uuid);
        ~auth_t();

        void authenticate(const std::string& message, const unsigned char* signature,
            size_t size, const std::string& token);

    private:
        EVP_MD_CTX* m_context;

        typedef std::map<const std::string, EVP_PKEY*> key_map_t;
        key_map_t m_keys;
};

}}

#endif
