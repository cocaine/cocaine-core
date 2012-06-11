/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

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
