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

#ifndef COCAINE_FORWARDS_HPP
#define COCAINE_FORWARDS_HPP

namespace cocaine {
    class context_t;

    namespace core {
        class core_t;
        class registry_t;
    }

    namespace crypto {
        class auth_t;
    }

    namespace engine {
        class engine_t;
        class manifest_t;

        namespace driver {        
            class driver_t;
        }

        namespace job {
            class job_t;
        }
        
        namespace slave {
            class slave_t;
        }
        
        class overseer_t;
    }

    namespace logging {
        class sink_t;
        class emitter_t;
    }

    namespace plugin {
        class module_t;
    }

    namespace storage {
        class storage_t;
    }
}

#endif
