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

#ifndef COCAINE_IO_SLOT_HPP
#define COCAINE_IO_SLOT_HPP

#include "cocaine/api/stream.hpp"

#include "cocaine/traits.hpp"

namespace cocaine {

class dispatch_t;

namespace detail {

// Slot basics

struct slot_concept_t {
    slot_concept_t(const std::string& name):
        m_name(name)
    { }

    virtual
   ~slot_concept_t() {
       // Empty.
    }

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) = 0;

public:
    std::string
    name() const {
        return m_name;
    }

private:
    const std::string m_name;
};

}

template<class Event>
struct basic_slot:
    public detail::slot_concept_t
{
    basic_slot():
        slot_concept_t(Event::alias())
    { }
};

} // namespace cocaine

#endif
