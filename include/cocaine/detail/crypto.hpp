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

#ifndef COCAINE_AUTH_HPP
#define COCAINE_AUTH_HPP

#include "cocaine/common.hpp"

#include "cocaine/api/storage.hpp"

#define PROTOTYPES
#include <mutils/mincludes.h>
#include <mutils/mhash.h>

namespace cocaine {

template<hashid HashID>
class crypto {
    COCAINE_DECLARE_NONCOPYABLE(crypto)

    const std::unique_ptr<logging::logger_t> m_log;
    const std::string m_service;

    api::storage_ptr m_store;

public:
    crypto(context_t& context, const std::string& service);
   ~crypto();

    std::string
    sign(const std::string& message, const std::string& token_id) const;
};

typedef crypto<MHASH_MD5> crypto_t;

} // namespace cocaine

#endif
