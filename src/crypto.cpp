/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/crypto.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

template<hashid HashID>
crypto<HashID>::crypto(context_t& context, const std::string& service):
    m_log(new logging::log_t(context, "crypto")),
    m_service(service)
{
    m_store = api::storage(context, "secure");
}

template<hashid HashID>
crypto<HashID>::~crypto() {
    // Empty.
}

template<hashid HashID>
std::string
crypto<HashID>::sign(const std::string& message, const std::string& token_id) const {
    std::string token;

    try {
        token = m_store->template get<std::string>(m_service, token_id);
    } catch(const storage_error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to locate the security token for '%s' in '%s'", token_id, m_service);
        throw cocaine::error_t("the specified token has not been found");
    }

    COCAINE_LOG_DEBUG(m_log, "signing a message for service '%s' with token '%s'", m_service, token_id);

    MHASH thread = mhash_hmac_init(HashID, const_cast<char*>(token.data()), token.size(), mhash_get_hash_pblock(HashID));
    char* digest = static_cast<char*>(::alloca(mhash_get_block_size(HashID)));

    mhash(thread, message.data(), message.size());
    mhash_hmac_deinit(thread, digest);

    return std::string(digest, mhash_get_block_size(HashID));
}

template class cocaine::crypto<MHASH_MD5>;
