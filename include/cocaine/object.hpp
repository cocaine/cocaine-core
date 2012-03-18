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

#ifndef COCAINE_OBJECT_HPP
#define COCAINE_OBJECT_HPP

#include "cocaine/common.hpp"

namespace cocaine {

// Basic object
// ------------

class object_t {
    public:        
        virtual ~object_t() = 0;

    protected:
        object_t(context_t& context);

    public:
        inline context_t& context() {
            return m_context;
        }

    private:
        context_t& m_context;
};

}

#endif
