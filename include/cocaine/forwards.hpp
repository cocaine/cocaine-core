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

#ifndef COCAINE_FORWARDS_HPP
#define COCAINE_FORWARDS_HPP

namespace cocaine {
    struct config_t;    
    class context_t;
    class repository_t;

    namespace crypto {
        class auth_t;
    }

    namespace engine {
        class engine_t;
        class master_t;
        struct job_t;
        struct manifest_t;

        namespace drivers {        
            class driver_t;
        }
    }

    namespace logging {
        class logger_t;
        class sink_t;
    }

    namespace storages {
        class storage_t;
    }
}

namespace zmq {
    class context_t;
    class message_t;
}

#endif
