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

#include "cocaine/logging.hpp"

namespace cocaine { namespace logging {

class syslog_t:
    public sink_t
{
    public:
        syslog_t(const std::string& identity, priorities verbosity);

        virtual void emit(priorities priority,
                          const std::string& message) const;

    private:
        const std::string m_identity;
        priorities m_verbosity;
};

}}
