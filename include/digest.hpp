#ifndef YAPPI_DIGEST_HPP
#define YAPPI_DIGEST_HPP

#include <openssl/evp.h>

#include "common.hpp"

namespace yappi { namespace security {

class digest_t:
    public boost::noncopyable
{
    public:
        digest_t();
        ~digest_t();

        std::string get(const std::string& data);

    private:
        EVP_MD_CTX* m_context;
};

}}

#endif
