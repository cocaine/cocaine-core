#ifndef YAPPI_DIGEST_HPP
#define YAPPI_DIGEST_HPP

#include <string>
#include <sstream>

#include <openssl/evp.h>

namespace yappi { namespace helpers {

class digest_t {
    public:
        digest_t():
            m_context(EVP_MD_CTX_create())
        {}

        ~digest_t() {
            EVP_MD_CTX_destroy(m_context);
        }

        std::string get(const std::string& data) {
            EVP_DigestInit_ex(m_context, EVP_md5(), NULL);
            EVP_DigestUpdate(m_context, data.data(), data.length());
            EVP_DigestFinal_ex(m_context, m_hash, &m_size);

            std::ostringstream formatter;
            for(unsigned int i = 0; i < m_size; i++) {
                formatter << std::hex << static_cast<unsigned int>(m_hash[i]);
            }

            return formatter.str();
        }

    private:
        EVP_MD_CTX* m_context;
        unsigned int m_size;
        unsigned char m_hash[EVP_MAX_MD_SIZE];
};

}}

#endif
