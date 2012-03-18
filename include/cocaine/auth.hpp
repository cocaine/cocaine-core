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

#ifndef COCAINE_AUTH_HPP
#define COCAINE_AUTH_HPP

#include <openssl/evp.h>

#include "cocaine/common.hpp"

#include "cocaine/helpers/blob.hpp"

namespace cocaine { namespace crypto {

    
class auth_t:
    public boost::noncopyable
{
    public:
        auth_t(context_t& ctx);
        ~auth_t();

        void verify(const blob_t& message,
                    const blob_t& signature,
                    const std::string& username);

        // std::string sign(const std::string& message, 
        //                  const std::string& username);

    private:
        boost::shared_ptr<logging::logger_t> m_log;

        EVP_MD_CTX * m_context;

        typedef std::map<const std::string, EVP_PKEY*> key_map_t;
        key_map_t m_keys;
};

}}

#endif
