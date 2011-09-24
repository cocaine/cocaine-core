#include <sstream>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "cocaine/storage.hpp"
#include "cocaine/security/signatures.hpp"

using namespace cocaine::security;

signatures_t::signatures_t():
    m_context(EVP_MD_CTX_create())
{
    // Initialize error strings
    ERR_load_crypto_strings();

    // Load the credentials
    // NOTE: Allowing the exception to propagate here, as this is a fatal error
    Json::Value keys(storage::storage_t::instance()->all("keys"));
    Json::Value::Members names(keys.getMemberNames());

    for(Json::Value::Members::const_iterator it = names.begin(); it != names.end(); ++it) {
        std::string identity(*it);
        Json::Value object(keys[identity]);

        if(!object["key"].isString() || object["key"].empty()) {
            syslog(LOG_ERR, "security: key for user '%s' is malformed", identity.c_str());
            continue;
        }

        std::string key(object["key"].asString());

        // Read the key into the BIO object
        BIO* bio = BIO_new_mem_buf(const_cast<char*>(key.data()), key.length());
        EVP_PKEY* pkey = NULL;
        
        pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
            
        if(pkey != NULL) {
            m_keys.insert(std::make_pair(identity, pkey));
        } else { 
            syslog(LOG_ERR, "security: key for user '%s' is invalid - %s",
                identity.c_str(), ERR_reason_error_string(ERR_get_error()));
        }

        BIO_free(bio);
    }
    
    syslog(LOG_NOTICE, "security: loaded %u public key(s)", m_keys.size());
}

signatures_t::~signatures_t() {
    for(key_map_t::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
        EVP_PKEY_free(it->second);
    }

    ERR_free_strings();
    EVP_MD_CTX_destroy(m_context);
}

/* XXX: Gotta invent something sophisticated here
std::string signatures_t::sign(const std::string& message, const std::string& token)
{
    key_map_t::const_iterator it = m_private_keys.find(token);

    if(it == m_private_keys.end()) {
        throw std::runtime_error("unauthorized user");
    }
 
    unsigned char buffer[EVP_PKEY_size(it->second)];
    unsigned int size = 0;
    
    EVP_SignInit(m_context, EVP_sha1());
    EVP_SignUpdate(m_context, message.data(), message.size());
    EVP_SignFinal(m_context, buffer, &size, it->second);
    EVP_MD_CTX_cleanup(m_context);

    return std::string(reinterpret_cast<char*>(buffer), size);
}
*/

void signatures_t::verify(const std::string& message, const unsigned char* signature,
                         unsigned int size, const std::string& token)
{
    key_map_t::const_iterator it(m_keys.find(token));

    if(it == m_keys.end()) {
        throw std::runtime_error("unauthorized user");
    }
    
    // Initialize the verification context
    EVP_VerifyInit(m_context, EVP_sha1());

    // Fill it with data
    EVP_VerifyUpdate(m_context, message.data(), message.length());
    
    // Verify the signature
    if(!EVP_VerifyFinal(m_context, signature, size, it->second)) {
        EVP_MD_CTX_cleanup(m_context);
        throw std::runtime_error("invalid signature");
    }

    EVP_MD_CTX_cleanup(m_context);
}
