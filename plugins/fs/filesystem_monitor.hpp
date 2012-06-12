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

#ifndef COCAINE_FILESYSTEM_MONITOR_DRIVER_HPP
#define COCAINE_FILESYSTEM_MONITOR_DRIVER_HPP

#include "cocaine/common.hpp"

// Has to be included after common.h
#include <ev++.h>

#include "cocaine/interfaces/driver.hpp"

namespace cocaine { namespace engine { namespace drivers {

class filesystem_monitor_t:
    public driver_t
{
    public:
        typedef driver_t category_type;

    public:
        filesystem_monitor_t(context_t& context,
                             engine_t& engine,
                             const plugin_config_t& config);

        virtual ~filesystem_monitor_t();

        virtual Json::Value info() const;

    private:
        void event(ev::stat&, int);

    private:
        const std::string m_event,
                          m_path;

        ev::stat m_watcher;
};

}}}

#endif
