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

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

context_t::context_t(config_t config_, boost::shared_ptr<logging::sink_t> sink):
    config(config_),
    m_io(new zmq::context_t(1)),
    m_log(sink)
{
    // Fetching the hostname
    const int HOSTNAME_MAX_LENGTH = 256;
    char hostname[HOSTNAME_MAX_LENGTH];

    if(gethostname(hostname, HOSTNAME_MAX_LENGTH) == 0) {
        config.core.hostname = hostname;
    } else {
        throw std::runtime_error("failed to determine the hostname");
    }
}

