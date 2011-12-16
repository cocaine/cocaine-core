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

#ifndef COCAINE_DRIVER_ZEROMQ_SINK_HPP
#define COCAINE_DRIVER_ZEROMQ_SINK_HPP

#include "cocaine/drivers/zeromq_server.hpp"

namespace cocaine { namespace engine { namespace driver {

class zeromq_sink_t:
    public zeromq_server_t
{
    public:
        zeromq_sink_t(engine_t& engine,
                      const std::string& method, 
                      const Json::Value& args);

        // Driver interface
        virtual Json::Value info() const;

    private:
        // Server interface
        virtual void process(ev::idle&, int);
};

}}}

#endif
