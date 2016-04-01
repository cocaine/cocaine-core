/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_CONTEXT_MAPPER_HPP
#define COCAINE_CONTEXT_MAPPER_HPP

#include "cocaine/common.hpp"

#include <deque>
#include <map>
#include <mutex>

namespace cocaine {

// Dynamic port mapper

class port_mapping_t {
    const
    std::map<std::string, port_t> m_pinned;

    // Ports available for dynamic allocation.
    std::deque<port_t> m_shared;
    std::mutex m_mutex;

    // Ports currently in use.
    std::map<std::string, port_t> m_in_use;

public:
    explicit
    port_mapping_t(const struct config_t& config);

    // Modifiers

    port_t
    assign(const std::string& name);

    void
    retain(const std::string& name);
};

} // namespace cocaine

#endif
