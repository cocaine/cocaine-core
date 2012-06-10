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

#ifndef COCAINE_RECURRING_TIMER_DRIVER_HPP
#define COCAINE_RECURRING_TIMER_DRIVER_HPP

#include "cocaine/common.hpp"

// Has to be included after common.h
#include <ev++.h>

#include "cocaine/interfaces/driver.hpp"

namespace cocaine { namespace engine { namespace drivers {

class recurring_timer_t:
    public driver_t
{
    public:
        typedef driver_t category_type;

    public:
        recurring_timer_t(context_t& context,
                          engine_t& engine,
                          const plugin_config_t& config);

        virtual ~recurring_timer_t();

        // Driver interface.
        virtual Json::Value info() const;

    private:
        void event(ev::timer&, int);

        // Timer interface.
        virtual void reschedule();

    protected:
        const std::string m_event;
        const double m_interval;

        ev::timer m_watcher;
};

}}}

#endif
