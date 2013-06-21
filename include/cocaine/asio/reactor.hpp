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
        m_loop_queue_pump(new ev::prepare(*m_loop)),
        m_loop_async_wake(new ev::async(*m_loop))
    {
        // Pumps queued jobs on beginning of each loop iteration.
        m_loop_queue_pump->set<reactor_t, &reactor_t::process>(this);
        m_loop_queue_pump->start();

        // Wakeups the loop when new jobs are queued.
        m_loop_async_wake->set<reactor_t, &reactor_t::wakeup>(this);
        m_loop_async_wake->start();
    }

   ~reactor_t() {
        m_loop_async_wake->stop();
        m_loop_queue_pump->stop();
    }

    void
    run() {
        m_loop->loop();
    }

    void
    run_with_timeout(float timeout) {
        update();

        throw_action_t action = { *this };
        ev::timer timeout_guard(*m_loop);

        timeout_guard.set(&action);
        timeout_guard.start(timeout);

        m_loop->loop();

        timeout_guard.stop();
    }

    void
    stop() {
        m_loop->unloop(ev::ALL);
    }

    void
    post(const job_type& job) {
        std::unique_lock<std::mutex> lock(m_job_queue_mutex);

        // Push the new job into the queue.
        m_job_queue.push_back(job);

        if(m_job_queue.size() == 1) {
            lock.unlock();

            // Wake up the event loop, in case it's the only job in the queue,
            // otherwise it's probably already awake.
            m_loop_async_wake->send();
        }
    }

    void
    update() {
        ev_now_update(*m_loop);
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

    void
    wakeup(ev::async&, int) {
        // Pass.
    }

private:
    struct throw_action_t {
        void
        operator()(ev::timer&, int) {
            self.stop();

            // This will destroy the timer in the run() stack frame as well.
            throw cocaine::error_t("timed out");
        }

        reactor_t& self;
    };

    std::unique_ptr<native_type> m_loop;
    std::unique_ptr<ev::prepare> m_loop_queue_pump;
    std::unique_ptr<ev::async>   m_loop_async_wake;

    std::deque<job_type> m_job_queue;
    std::mutex m_job_queue_mutex;
};

}} // namespace cocaine::io

#endif
