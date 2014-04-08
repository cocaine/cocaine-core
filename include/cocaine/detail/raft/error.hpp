/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_RAFT_ERROR_HPP
#define COCAINE_RAFT_ERROR_HPP

#include <system_error>
#include <stdexcept>
#include <exception>
#include <type_traits>
#include <string>

namespace cocaine {

enum class raft_errc {
    // Only leader can append an entry to the log.
    // If not-leader tries to append an entry, algorithm returns this error code to callback.
    not_leader = 1,

    // When leader becomes follower, some uncommitted entries may eventually become committed or discarded.
    // So, when leader becomes follower, it returns this error code to callbacks of all uncommitted entries.
    unknown,

    // State machine replies with this code on configuration changes commands,
    // when configuration is in transitional state. Configuration changes protocol allows
    // only one operation at the same time.
    busy
};

struct raft_category_t :
    public std::error_category
{
    const char*
    name() const throw() {
        return "RAFT";
    }

    std::string
    message(int error_value) const throw() {
        switch (error_value) {
            case static_cast<int>(raft_errc::not_leader):
                return "The node is not a leader";
            case static_cast<int>(raft_errc::unknown):
                return "Status of the request is unknown";
            case static_cast<int>(raft_errc::busy):
                return "Some cluster change is in cluster";
            default:
                return "Unexpected RAFT error";
        }
    }
};

inline
const std::error_category&
raft_category() {
    static raft_category_t category_instance;
    return category_instance;
}

std::error_code
make_error_code(raft_errc e);

std::error_condition
make_error_condition(raft_errc e);

} // namespace cocaine

namespace std {

template<>
struct is_error_code_enum<cocaine::raft_errc> :
    public true_type
{ };

} // namespace std

#endif // COCAINE_RAFT_ERROR_HPP
