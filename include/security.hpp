#ifndef YAPPI_SECURITY_HPP
#define YAPPI_SECURITY_HPP

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>

#include "common.hpp"

namespace yappi { namespace helpers {

namespace fs = boost::filesystem;

class security_t: public boost::noncopyable {
    public:
        security_t(const std::string& credentials_path):
            m_context(EVP_MD_CTX_create())
        {
            // Initialize the verification context
            EVP_VerifyInit(m_context, EVP_sha1());
        
            // Load the credentials
            Json::Value& root;
            fs::ifstream stream;
            fs::path path(credentials_path);

            stream.exceptions(std::ofstream::badbit | std::ofstream::failbit);

            try {
                stream.open(path, fs::ifstream::in);
            } catch(const fs::ifstream::failure& e) {
                throw std::runtime_error("cannot load security credentials from " +
                    path.string());
            }

            if(!reader.parse(stream, root)) {
                throw std::runtime_error("security: malformed json in " + path.string() +
                    " - " + reader.getFormatedErrorMessages());
            }

            for(Json::Value::const_iterator it = root.begin(); it != root.end(); ++it) {
                Json::Value object = *it;

                std::string identity = object["identity"];
                std::string key = object["key"];

                BIO* bio = BIO_new_mem_buf(key.data(), key.length());
                RSA* rsa = PEM_read_bio_RSAPublicKey(bio, NULL, NULL, NULL);
                
                EVP_PKEY* pkey = EVP_PKEY_new();
                EVP_PKEY_set1_RSA(pkay, rsa);
                
                m_keys->insert(std::make_pair(identity, pkey));

                RSA_free(rsa);
                BIO_free(bio);
            }
        }

        ~security_t() {
            for(key_list_t::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
                EVP_PKEY_free(it->second);
            }

            EVP_MD_CTX_destroy(m_context);
        }

        void authenticate(const std::string& m, const std::string& s, const std::string& u) {
            key_map_t::const_iterator it = m_keys.find(u);

            if(it == m_keys.end()) {
                throw std::runtime_error("unauthorized user");
            }
            
            EVP_VerifyUpdate(m_context, m.data(), m.length());
            
            if(!EVP_VerifyFinal(m_context, s.data(), s.length(), it->second)) {
                throw std::runtime_error("invalid user signature");
            }
        }

    private:
        EVP_MD_CTX* m_context;

        typedef std::map<const std::string, EVP_PKEY*> key_map_t;
        key_map_t m_keys;
};

}}

#endif
