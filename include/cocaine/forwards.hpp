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
    namespace engine {
        class engine_t;
        class overseer_t;

        namespace driver {        
            class driver_t;
        }

        namespace job {
            class job_t;
        }
        
        namespace slave {
            class slave_t;
        }
    }

    namespace plugin {
        class source_t;
    }
}

#endif
