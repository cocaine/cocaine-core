#include <sstream>
#include <iomanip>

#include "digest.hpp"

using namespace yappi::security;

digest_t::digest_t():
    m_context(EVP_MD_CTX_create())
{}

digest_t::~digest_t() {
    EVP_MD_CTX_destroy(m_context);
}

std::string digest_t::get(const std::string& data) {
    unsigned int size;
    unsigned char hash[EVP_MAX_MD_SIZE];
    
    EVP_DigestInit(m_context, EVP_sha1());
    EVP_DigestUpdate(m_context, data.data(), data.length());
    EVP_DigestFinal(m_context, hash, &size);
    EVP_MD_CTX_cleanup(m_context);

    std::ostringstream formatter;

    for(unsigned int i = 0; i < size; i++) {
        formatter << std::hex << std::setw(2) << std::setfill('0')
                  << hash[i];
    }

    return formatter.str();
}
