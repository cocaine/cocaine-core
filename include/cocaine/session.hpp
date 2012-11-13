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

#ifndef COCAINE_SESSION_HPP
#define COCAINE_SESSION_HPP

#include <boost/thread/mutex.hpp>

#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 4)
 #include <cstdatomic>
#else
 #include <atomic>
#endif

#include "cocaine/common.hpp"
#include "cocaine/unique_id.hpp"

namespace cocaine { namespace engine {

struct session_t {
    enum states: int {
        inactive,
        active,
        closed
    };

public:
    session_t(const boost::shared_ptr<event_t>& event,
              engine_t * const engine);

    ~session_t();

    void
    attach(slave_t * const slave);

    void
    push(const void * chunk,
         size_t size);

    void
    close();

public:
    // Job state.
    std::atomic<int> state;

    // Job ID.
    const unique_id_t id;

    // Tracked event itself.
    const boost::shared_ptr<event_t> ptr;

private:
    engine_t * const m_engine;

    typedef std::vector<
        std::string
    > chunk_list_t;

    // Request chunk cache.
    chunk_list_t m_cache;
    boost::mutex m_mutex;

    // Responsible slave.
    slave_t * m_slave;
};

}}

#endif
