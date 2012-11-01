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
#include <boost/weak_ptr.hpp>

#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 4)
 #include <cstdatomic>
#else
 #include <atomic>
#endif

#include "cocaine/common.hpp"
#include "cocaine/events.hpp"
#include "cocaine/unique_id.hpp"

namespace cocaine { namespace engine {

struct session_t {
    struct state {
        enum value: int {
            unknown,
            processing,
            complete
        };
    };

public:
    session_t(const boost::shared_ptr<job_t>& job_);
    ~session_t();

    void
    process(const events::invoke&);
    
    void
    process(const events::chunk&);
    
    void
    process(const events::error&);
    
    void
    process(const events::choke&);

    void
    push(const std::string& chunk);

public:
    // Current session state.
    std::atomic<int> state;

    // Session ID.
    const unique_id_t id;

    // Tracked job.
    boost::shared_ptr<job_t> job;

private:
    boost::mutex m_mutex;
    
    // Request chunk cache.

    typedef std::vector<
        std::string
    > chunk_list_t;

    chunk_list_t m_cache;

    // Weak reference to job's master.
    boost::weak_ptr<master_t> m_master;
};

}} // namespace cocaine::engine

#endif
