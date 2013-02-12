/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/auth.hpp"

#include "cocaine/api/storage.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>

using namespace cocaine::crypto;
using namespace cocaine::logging;

auth_t::auth_t(context_t& context):
    m_context(context),
    m_log(new log_t(context, "crypto")),
    m_evp_md_context(EVP_MD_CTX_create())
{
    ERR_load_crypto_strings();

    auto storage = api::storage(context, "core");

    std::vector<std::string> keys;

    try {
        keys = storage->list("keys");
    } catch(const storage_error_t& e) {
        COCAINE_LOG_WARNING(m_log, "unable to read the key directory - %s", e.what());
        return;
    }

    for(std::vector<std::string>::const_iterator it = keys.begin();
        it != keys.end();
        ++it)
    {
        const std::string identity = *it;
        const std::string object = storage->get<std::string>("keys", identity);

        if(object.empty()) {
            COCAINE_LOG_ERROR(m_log, "key for user '%s' is malformed", identity);
            continue;
        }

        // Read the key into the BIO object.
        BIO * bio = BIO_new_mem_buf(const_cast<char*>(object.data()), object.size());
        EVP_PKEY * pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);

        if(pkey != NULL) {
            m_keys.emplace(identity, pkey);
        } else {
            COCAINE_LOG_ERROR(
                m_log,
                "key for user '%s' is invalid - %s",
                identity,
                ERR_reason_error_string(ERR_get_error())
            );
        }

        BIO_free(bio);
    }

    COCAINE_LOG_INFO(m_log, "loaded %llu public key(s)", m_keys.size());
}

namespace {
    struct dispose_t {
        template<class T>
        void
        operator()(T& key) const {
            EVP_PKEY_free(key.second);
        }
    };
}

auth_t::~auth_t() {
    std::for_each(m_keys.begin(), m_keys.end(), dispose_t());
    ERR_free_strings();
    EVP_MD_CTX_destroy(m_evp_md_context);
}

/* TODO: Gotta invent something sophisticated here.
std::string auth_t::sign(const std::string& message,
                         const std::string& username) const
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

void
auth_t::verify(const std::string& message,
               const std::string& signature,
               const std::string& username) const
{
    key_map_t::const_iterator it(m_keys.find(username));

    if(it == m_keys.end()) {
        throw authorization_error_t("unauthorized user");
    }

    EVP_VerifyInit(m_evp_md_context, EVP_sha1());
    EVP_VerifyUpdate(m_evp_md_context, message.data(), message.size());

    bool success = EVP_VerifyFinal(
        m_evp_md_context,
        reinterpret_cast<const unsigned char*>(signature.data()),
        signature.size(),
        it->second
    );

    if(!success) {
        EVP_MD_CTX_cleanup(m_evp_md_context);
        throw authorization_error_t("invalid signature");
    }

    EVP_MD_CTX_cleanup(m_evp_md_context);
}
