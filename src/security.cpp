#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "security.hpp"

using namespace yappi::security;

auth_t::auth_t(const std::string& uuid):
    m_context(EVP_MD_CTX_create())
{
    // Initialize error strings
    ERR_load_crypto_strings();

    // Load the credentials
    Json::Value root;
    Json::Reader reader(Json::Features::strictMode());
    
    fs::ifstream stream;
    fs::path path = fs::path("/var/lib/yappi") / (uuid + ".credentials");

    stream.exceptions(std::ofstream::badbit | std::ofstream::failbit);

    try {
        stream.open(path, fs::ifstream::in);
    } catch(const fs::ifstream::failure& e) {
        throw std::runtime_error("cannot load security credentials from " +
            path.string());
    }

    if(!reader.parse(stream, root)) {
        throw std::runtime_error("malformed json in " + path.string() +
            " - " + reader.getFormatedErrorMessages());
    }

    for(Json::Value::iterator it = root.begin(); it != root.end(); ++it) {
        Json::Value object = *it;

        std::string identity = object["identity"].asString();
        std::string key = object["key"].asString();

        BIO* bio = BIO_new_mem_buf(const_cast<char*>(key.data()), key.length());
        EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);

        if(pkey == NULL) {
            syslog(LOG_ERR, "security: failed to load public key for %s - %s",
                identity.c_str(), ERR_reason_error_string(ERR_get_error()));
            continue;
        }

        m_keys.insert(std::make_pair(identity, pkey));

        BIO_free(bio);
    }
    
    syslog(LOG_NOTICE, "security: initialized %d credentials", m_keys.size());
}

auth_t::~auth_t() {
    for(key_map_t::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
        EVP_PKEY_free(it->second);
    }

    EVP_MD_CTX_destroy(m_context);
    ERR_free_strings();
}

void auth_t::authenticate(const std::string& message, const unsigned char* signature,
                          size_t size, const std::string& token)
{
    key_map_t::const_iterator it = m_keys.find(token);

    if(it == m_keys.end()) {
        throw std::runtime_error("unauthorized user");
    }
    
    // Initialize the verification context
    EVP_VerifyInit(m_context, EVP_sha1());

    // Fill it with data
    EVP_VerifyUpdate(m_context, message.data(), message.length());
    
    // Verify the signature
    if(!EVP_VerifyFinal(m_context, signature, size, it->second)) {
        throw std::runtime_error("invalid signature");
    }

    EVP_MD_CTX_cleanup(m_context);
}
