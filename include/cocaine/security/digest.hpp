#ifndef COCAINE_DIGEST_HPP
#define COCAINE_DIGEST_HPP

#include <openssl/evp.h>

#include "cocaine/common.hpp"

namespace cocaine { namespace security {

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
