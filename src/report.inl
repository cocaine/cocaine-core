/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

struct context_t::memusage_action_t {
    typedef event_traits<io::locator::reports>::result_type result_type;

    result_type
    operator()() const;

    const context_t& self;
};

auto
context_t::memusage_action_t::operator()() const -> result_type {
    result_type result;

    std::lock_guard<std::mutex> guard(self.m_mutex);

    for(auto it = self.m_services.begin(); it != self.m_services.end(); ++it) {
        io::locator::reports::usage_report_type report;

        // Get the usage counters from the service's actor.
        const auto source = it->second->counters();

        for(auto channel = source.footprints.begin(); channel != source.footprints.end(); ++channel) {
            auto& endpoint = channel->first;
            auto  consumed = channel->second;

            // Convert I/O endpoints to endpoint tuples. That's the only reason why this function
            // exists at all, as opposed to returning the counters as is.
            report.insert({
                io::locator::endpoint_tuple_type(endpoint.address().to_string(), endpoint.port()),
                consumed
            });
        }

        result[it->first] = std::make_tuple(source.sessions, report);
    }

    return result;
}
