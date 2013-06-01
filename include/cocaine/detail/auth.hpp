/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_AUTH_HPP
#define COCAINE_AUTH_HPP

#include "cocaine/common.hpp"

#include <openssl/evp.h>

namespace cocaine {

struct authorization_error_t:
    public error_t
{
    template<typename... Args>
    authorization_error_t(const std::string& format, const Args&... args):
        error_t(format, args...)
    { }
};

namespace crypto {

class auth_t {
    COCAINE_DECLARE_NONCOPYABLE(auth_t)

    public:
        auth_t(context_t& context);
       ~auth_t();

        void
        verify(const std::string& message,
               const std::string& signature,
               const std::string& username) const;

        // std::string
        // sign(const std::string& message,
        //      const std::string& username) const;

    private:
        std::unique_ptr<logging::log_t> m_log;

        EVP_MD_CTX* m_evp_md_context;

        typedef std::map<
            const std::string,
            EVP_PKEY*
        > key_map_t;

        key_map_t m_keys;
};

}} // namespace cocaine::crypto

#endif
