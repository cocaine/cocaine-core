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

#ifndef COCAINE_IO_REACTOR_HPP
#define COCAINE_IO_REACTOR_HPP

#include "cocaine/common.hpp"

#include <deque>
#include <functional>
#include <mutex>

#include <ev++.h>

namespace cocaine { namespace io {

struct reactor_t {
    COCAINE_DECLARE_NONCOPYABLE(reactor_t)

    typedef ev::dynamic_loop native_type;
    typedef std::function<void()> job_type;

    reactor_t():
        m_loop(new ev::dynamic_loop()),
        m_loop_prepare(new ev::prepare(*m_loop))
    {
        m_loop_prepare->set<reactor_t, &reactor_t::process>(this);
        m_loop_prepare->start();
    }

   ~reactor_t() {
        m_loop_prepare->stop();
    }

    void
    run() {
        m_loop->loop();
    }

    void
    stop() {
        m_loop->unloop(ev::ALL);
    }

    void
    post(const job_type& job) {
        std::lock_guard<std::mutex> guard(m_job_queue_mutex);
        m_job_queue.push_back(job);
    }

public:
    native_type&
    native() {
        return *m_loop;
    }

private:
    void
    process(ev::prepare&, int) {
        std::lock_guard<std::mutex> guard(m_job_queue_mutex);

        for(auto it = m_job_queue.begin(); it != m_job_queue.end(); ++it) {
            (*it)();
        }

        m_job_queue.clear();
    }

private:
    std::unique_ptr<native_type> m_loop;
    std::unique_ptr<ev::prepare> m_loop_prepare;

    std::deque<job_type> m_job_queue;
    std::mutex m_job_queue_mutex;
};

}} // namespace cocaine::io

#endif
