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

#include "cocaine/context/mapper.hpp"

#include "cocaine/context/config.hpp"
#include "cocaine/errors.hpp"

#include <map>
#include <numeric>
#include <random>

using namespace cocaine;

port_mapping_t::port_mapping_t(const config_t& config):
    m_pinned(config.network().ports().pinned())
{
    port_t minimum, maximum;

    std::tie(minimum, maximum) = config.network().ports().shared();

    std::vector<port_t> seed;

    if((minimum == 0 && maximum == 0) || maximum <= minimum) {
        seed.resize(65535);
        std::fill(seed.begin(), seed.end(), 0);
    } else {
        seed.resize(maximum - minimum);
        std::iota(seed.begin(), seed.end(), minimum);
    }

    std::random_device device;
    std::shuffle(seed.begin(), seed.end(), std::default_random_engine(device()));

    // Populate the shared port queue.
    m_shared = std::deque<port_t>(seed.begin(), seed.end());
}

port_t
port_mapping_t::assign(const std::string& name) {
    std::lock_guard<std::mutex> guard(m_mutex);

    if(m_in_use.count(name)) {
        throw cocaine::error_t("named port for {} is already in use", name);
    }

    if(m_pinned.count(name)) {
        return m_in_use.insert({name, m_pinned.at(name)}).first->second;
    }

    if(m_shared.empty()) {
        throw cocaine::error_t("no ports left for allocation");
    }

    const auto port = m_shared.front(); m_shared.pop_front();

    return m_in_use.insert({name, port}).first->second;
}

void
port_mapping_t::retain(const std::string& name) {
    std::lock_guard<std::mutex> guard(m_mutex);

    if(!m_in_use.count(name)) {
        throw cocaine::error_t("named port was never assigned");
    }

    if(!m_pinned.count(name)) {
        m_shared.push_back(m_in_use.at(name));
    }

    m_in_use.erase(name);
}
