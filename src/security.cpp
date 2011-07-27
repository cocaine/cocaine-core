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
    fs::path path = fs::path("/var/lib/yappi/" + uuid + ".tokens");
   
    if(fs::exists(path) && !fs::is_directory(path)) {
        throw std::runtime_error(path.string() + " is not a directory");
    }

    if(!fs::exists(path)) {
        try {
            fs::create_directories(path);
        } catch(const std::runtime_error& e) {
            throw std::runtime_error("cannot create " + path.string());
        }
    }

    fs::directory_iterator it(path), end;

    while(it != end) {
        if(fs::is_regular(it->status())) {
            fs::ifstream stream;
            stream.exceptions(std::ofstream::badbit | std::ofstream::failbit);

            try {
                stream.open(it->path(), fs::ifstream::in);
            } catch(const fs::ifstream::failure& e) {
                syslog(LOG_ERR, "security: cannot open %s", it->path().string().c_str());
                ++it;
                continue;
            }

            std::string filename = it->leaf();
            std::string identity = filename.substr(0, filename.find_last_of("."));
            std::ostringstream key;
            
            key << stream.rdbuf();
            BIO* bio = BIO_new_mem_buf(const_cast<char*>(key.str().data()), key.str().length());
            EVP_PKEY* rsa = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);

            if(rsa == NULL) {
                syslog(LOG_ERR, "security: failed to load public key from %s - %s",
                    it->path().string().c_str(), ERR_reason_error_string(ERR_get_error()));
            } else {
                m_keys.insert(std::make_pair(identity, rsa));
            }

            BIO_free(bio);
        }
        
        ++it;
    }
    
    syslog(LOG_NOTICE, "security: initialized %d credential(s)", m_keys.size());
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
