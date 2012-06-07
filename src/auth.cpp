//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
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
#include "cocaine/logging.hpp"

#include "cocaine/interfaces/storage.hpp"

#include "cocaine/helpers/json.hpp"

using namespace cocaine::crypto;
using namespace cocaine::storages;

auth_t::auth_t(context_t& context):
    m_log(context.log("crypto")),
    m_context(EVP_MD_CTX_create())
{
    ERR_load_crypto_strings();

    // NOTE: Allowing the exception to propagate here, as this is a fatal error.
    std::vector<std::string> keys(
        context.storage<objects>("core")->list("keys")
    );

    for(std::vector<std::string>::const_iterator it = keys.begin();
        it != keys.end();
        ++it)
    {
        std::string identity(*it);

        objects::value_type object(
            context.storage<objects>("core")->get("keys", identity)
        );

        if(object.blob.empty()) {
            m_log->error("key for user '%s' is malformed", identity.c_str());
            continue;
        }

        // Read the key into the BIO object.
        BIO * bio = BIO_new_mem_buf(const_cast<void*>(object.blob.data()), object.blob.size());
        EVP_PKEY * pkey = NULL;
        
        pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
            
        if(pkey != NULL) {
            m_keys.insert(std::make_pair(identity, pkey));
        } else { 
            m_log->error("key for user '%s' is invalid - %s",
                identity.c_str(), 
                ERR_reason_error_string(ERR_get_error())
            );
        }

        BIO_free(bio);
    }
    
    m_log->info("loaded %zu public key(s)", m_keys.size());
}

namespace {
    struct disposer {
        template<class T>
        void operator()(T& key) {
            EVP_PKEY_free(key.second);
        }
    };
}

auth_t::~auth_t() {
    std::for_each(m_keys.begin(), m_keys.end(), disposer());
    ERR_free_strings();
    EVP_MD_CTX_destroy(m_context);
}

/* XXX: Gotta invent something sophisticated here.
std::string auth_t::sign(const std::string& message, const std::string& username)
{
    key_map_t::const_iterator it = m_private_keys.find(username);

    if(it == m_private_keys.end()) {
        throw authorization_error_t("unauthorized user");
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

void auth_t::verify(const blob_t& message,
                    const blob_t& signature,
                    const std::string& username)
{
    key_map_t::const_iterator it(m_keys.find(username));

    if(it == m_keys.end()) {
        throw authorization_error_t("unauthorized user");
    }
    
    EVP_VerifyInit(m_context, EVP_sha1());
    EVP_VerifyUpdate(m_context, message.data(), message.size());
    
    bool success = EVP_VerifyFinal(
        m_context,
        static_cast<const unsigned char*>(signature.data()),
        signature.size(),
        it->second
    );

    if(!success) {
        EVP_MD_CTX_cleanup(m_context);
        throw authorization_error_t("invalid signature");
    }

    EVP_MD_CTX_cleanup(m_context);
}
