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

#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include <boost/thread/recursive_mutex.hpp>

#include "cocaine/common.hpp"
#include "cocaine/events.hpp"
#include "cocaine/policy.hpp"

#include "cocaine/helpers/birth_control.hpp"

namespace cocaine { namespace engine {

struct job_t:
    public birth_control<job_t>
{
    struct lock_t {
        lock_t(job_t& job):
            lock(job.mutex)
        { }

    private:
        boost::unique_lock<boost::recursive_mutex> lock;
    };

    typedef std::vector<
        std::string
    > chunk_list_t;

public:
    job_t(const std::string& event);

    job_t(const std::string& event,
          policy_t policy);
    
    virtual
    ~job_t();

    void
    push(const std::string& chunk);

public:
    void
    process(const events::invoke&);
    
    void
    process(const events::chunk&);
    
    void
    process(const events::error&);
    
    void
    process(const events::choke&);
    
public:
    virtual
    void
    react(const events::chunk&) { }
    
    virtual
    void
    react(const events::error&) { }
    
    virtual
    void
    react(const events::choke&) { }

public:
    size_t
    size() const;

    const chunk_list_t&
    chunks() const;

public:
    enum {
        unknown,
        processing,
        complete
    } state;

    // Wrapped event type.
    const std::string event;
    
    // Execution policy.
    const policy_t policy;

private:
    boost::recursive_mutex mutex;
    
    // Request chunk cache.
    chunk_list_t cache;

    // Weak reference to job's master.
    boost::weak_ptr<master_t> master;
};

}} // namespace cocaine::engine

#endif
