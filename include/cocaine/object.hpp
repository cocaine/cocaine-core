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

#ifndef COCAINE_OBJECT_HPP
#define COCAINE_OBJECT_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/logging.hpp"

namespace cocaine {

// Basic object
// ------------

class object_t:
    public boost::noncopyable
{
    public:
        object_t(context_t& context, const std::string& identity);
        virtual ~object_t();

    public:
        inline context_t& context() {
            return m_context;
        }

        inline logging::emitter_t& log() {
            return m_log;
        }

    private:
        context_t& m_context;
        logging::emitter_t m_log;
};

}

#endif
