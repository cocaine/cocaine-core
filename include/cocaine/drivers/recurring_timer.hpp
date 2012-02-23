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

#ifndef COCAINE_DRIVER_RECURRING_TIMER_HPP
#define COCAINE_DRIVER_RECURRING_TIMER_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace driver {

class recurring_timer_t:
    public driver_t
{
    public:
        recurring_timer_t(engine_t& engine,
                          const std::string& method, 
                          const Json::Value& args);

        virtual ~recurring_timer_t();

        // Driver interface
        virtual Json::Value info() const;

    private:
        void event(ev::timer&, int);

        // Timer interface
        virtual void reschedule();

    protected:
        const ev::tstamp m_interval;
        ev::timer m_watcher;
};

}}}

#endif
