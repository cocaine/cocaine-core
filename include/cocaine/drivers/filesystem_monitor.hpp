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

#ifndef COCAINE_FILESYSTEM_MONITOR_DRIVER_HPP
#define COCAINE_FILESYSTEM_MONITOR_DRIVER_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace drivers {

class filesystem_monitor_t:
    public driver_t
{
    public:
        filesystem_monitor_t(engine_t& engine,
                             const std::string& method, 
                             const Json::Value& args);

        virtual ~filesystem_monitor_t();

        virtual Json::Value info() /* const */;

    private:
        void event(ev::stat&, int);

    private:
        const std::string m_path;
        ev::stat m_watcher;
};

}}}

#endif
