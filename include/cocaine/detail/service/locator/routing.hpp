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

#ifndef COCAINE_LOCATOR_SERVICE_ROUTING_HPP
#define COCAINE_LOCATOR_SERVICE_ROUTING_HPP

#include "cocaine/common.hpp"

#include <random>

namespace cocaine { namespace service {

// Ketama algorithm implementation

struct continuum_t {
    typedef uint32_t point_type;

    typedef struct element_type {
        point_type  point;
        std::string value;
    } element_t;

    friend
    bool
    operator< (const element_t& lhs, const element_t& rhs) {
        return lhs.point < rhs.point;
    }

    friend
    bool
    operator==(const element_t& lhs, const element_t& rhs) {
        return lhs.point == rhs.point;
    }

    friend
    bool
    operator< (const point_type lhs, const element_t& rhs) {
        return lhs < rhs.point;
    }

    typedef std::map<std::string, unsigned int> stored_type;

public:
    continuum_t(std::unique_ptr<logging::log_t> log, const stored_type& group);

    // Observers

    std::string
    get(const std::string& key) const;

    std::string
    get() const;

    std::vector<std::tuple<point_type, std::string>>
    all() const;

private:
    // Shared to allow cloning of rg_map_t for routing group updates.
    const std::shared_ptr<logging::log_t> m_log;

    // The hashring.
    std::vector<element_t> m_elements;

    // Used for keyless operations.
    std::default_random_engine                mutable m_rng;
    std::uniform_int_distribution<point_type> mutable m_distribution;
};

}} // namespace cocaine::service

#endif
