#include <sstream>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "security.hpp"
#include "storage.hpp"

using namespace yappi::security;

signatures_t::signatures_t():
    m_context(EVP_MD_CTX_create())
{
    // Initialize error strings
    ERR_load_crypto_strings();

    // Load the credentials
    Json::Value keys;
   
    try { 
        keys = storage::storage_t::instance()->all("keys");   
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "security: storage failure - %s", e.what());
        return;
    }
        
    Json::Value::Members names = keys.getMemberNames();

    for(Json::Value::Members::const_iterator it = names.begin(); it != names.end(); ++it) {
        Json::Value object = keys[*it];

        std::string identity = object.get("identity", "").asString();
        std::string type = object.get("type", "").asString();
        std::string key = object.get("key", "").asString();

        if(identity.empty() || type.empty() || key.empty()) {
            syslog(LOG_WARNING, "security: malformed json in '%s'", it->c_str());
            continue;
        }

        // Read the key into the BIO object
        BIO* bio = BIO_new_mem_buf(const_cast<char*>(key.data()), key.length());
        EVP_PKEY* pkey = NULL;
        
        if(type == "public") {
            pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        } else if(type == "private") {
            pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
        } else {
            syslog(LOG_WARNING, "security: unknown key type - '%s'", type.c_str());
        }
            
        if(pkey != NULL) {
            if(type == "public") {
                m_public_keys.insert(std::make_pair(identity, pkey));
            } else {
                m_private_keys.insert(std::make_pair(identity, pkey));
            }
        } else { 
            syslog(LOG_ERR, "security: malformed key in '%s' - %s",
                it->c_str(), ERR_reason_error_string(ERR_get_error()));
        }

        BIO_free(bio);
        
        ++it;
    }
    
    syslog(LOG_NOTICE, "security: loaded %ld public key(s) and %ld private key(s)",
        m_public_keys.size(), m_private_keys.size());
}

signatures_t::~signatures_t() {
    for(key_map_t::iterator it = m_public_keys.begin(); it != m_public_keys.end(); ++it) {
        EVP_PKEY_free(it->second);
    }

    for(key_map_t::iterator it = m_private_keys.begin(); it != m_private_keys.end(); ++it) {
        EVP_PKEY_free(it->second);
    }
    
    ERR_free_strings();
    EVP_MD_CTX_destroy(m_context);
}

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

void signatures_t::verify(const std::string& message, const unsigned char* signature,
                         unsigned int size, const std::string& token)
{
    key_map_t::const_iterator it = m_public_keys.find(token);

    if(it == m_public_keys.end()) {
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
