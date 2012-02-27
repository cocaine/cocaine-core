//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "cocaine/auth.hpp"

#include "cocaine/context.hpp"
#include "cocaine/storages/base.hpp"

using namespace cocaine;
using namespace cocaine::crypto;

auth_t::auth_t(context_t& ctx):
    object_t(ctx, "auth"),
    m_md_context(EVP_MD_CTX_create())
{
    // Initialize error strings
    ERR_load_crypto_strings();

    // Load the credentials
    // NOTE: Allowing the exception to propagate here, as this is a fatal error.
    Json::Value keys(context().storage().all("keys"));
    Json::Value::Members names(keys.getMemberNames());

    for(Json::Value::Members::const_iterator it = names.begin();
        it != names.end();
        ++it) 
    {
        std::string identity(*it);
        Json::Value object(keys[identity]);

        if(!object["key"].isString() || object["key"].empty()) {
            log().error("key for user '%s' is malformed", identity.c_str());
            continue;
        }

        std::string key(object["key"].asString());

        // Read the key into the BIO object
        BIO* bio = BIO_new_mem_buf(const_cast<char*>(key.data()), key.size());
        EVP_PKEY* pkey = NULL;
        
        pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
            
        if(pkey != NULL) {
            m_keys.insert(std::make_pair(identity, pkey));
        } else { 
            log().error("key for user '%s' is invalid - %s",
                identity.c_str(), 
                ERR_reason_error_string(ERR_get_error())
            );
        }

        BIO_free(bio);
    }
    
    log().info("loaded %zu public key(s)", m_keys.size());
}

auth_t::~auth_t() {
    for(key_map_t::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
        EVP_PKEY_free(it->second);
    }

    ERR_free_strings();
    EVP_MD_CTX_destroy(m_md_context);
}

/* XXX: Gotta invent something sophisticated here
std::string auth_t::sign(const std::string& message, const std::string& username)
{
    key_map_t::const_iterator it = m_private_keys.find(username);

    if(it == m_private_keys.end()) {
        throw std::runtime_error("unauthorized user");
    }
 
    unsigned char buffer[EVP_PKEY_size(it->second)];
    unsigned int size = 0;
    
    EVP_SignInit(m_md_context, EVP_sha1());
    EVP_SignUpdate(m_md_context, message.data(), message.size());
    EVP_SignFinal(m_md_context, buffer, &size, it->second);
    EVP_MD_CTX_cleanup(m_md_context);

    return std::string(reinterpret_cast<char*>(buffer), size);
}
*/

void auth_t::verify(const char* message,
                          size_t message_size, 
                          const unsigned char* signature,
                          size_t signature_size, 
                          const std::string& username)
{
    key_map_t::const_iterator it(m_keys.find(username));

    if(it == m_keys.end()) {
        throw std::runtime_error("unauthorized user");
    }
    
    // Initialize the verification context
    EVP_VerifyInit(m_md_context, EVP_sha1());

    // Fill it with data
    EVP_VerifyUpdate(m_md_context, message, message_size);
    
    // Verify the signature
    if(!EVP_VerifyFinal(m_md_context, signature, signature_size, it->second)) {
        EVP_MD_CTX_cleanup(m_md_context);
        throw std::runtime_error("invalid signature");
    }

    EVP_MD_CTX_cleanup(m_md_context);
}

