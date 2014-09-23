/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/detail/chamber.hpp"

#if defined(__linux__)
    #include <sys/prctl.h>
#elif defined(__APPLE__)
    #include <pthread.h>
#endif

#include <sys/resource.h>

using namespace cocaine::io;

// Chamber internals

class chamber_t::named_runnable_t {
    const std::string name;
    const std::shared_ptr<boost::asio::io_service>& asio;

public:
    named_runnable_t(const std::string& name_, const std::shared_ptr<boost::asio::io_service>& asio_):
        name(name_),
        asio(asio_)
    { }

    void
    operator()() const;
};

void
chamber_t::named_runnable_t::operator()() const {
#if defined(__linux__)
    if(name.size() < 16) {
        ::prctl(PR_SET_NAME, name.c_str());
    } else {
        ::prctl(PR_SET_NAME, name.substr(0, 16).data());
    }
#elif defined(__APPLE__)
    pthread_setname_np(name.c_str());
#endif

    asio->run();
}

class chamber_t::stats_periodic_action_t:
    public std::enable_shared_from_this<stats_periodic_action_t>
{
    chamber_t* impl;
    const boost::posix_time::seconds interval;

    // Snapshot of the last getrusage(2) report to be able to calculate the difference.
    struct rusage last_tick;

public:
    template<class Interval>
    stats_periodic_action_t(chamber_t* impl_, Interval interval_):
        impl(impl_),
        interval(interval_)
    {
        std::memset(&last_tick, 0, sizeof(last_tick));
    }

    void
    operator()();

private:
    void
    finalize(const boost::system::error_code& ec);
};

void
chamber_t::stats_periodic_action_t::operator()() {
    impl->cron->expires_from_now(interval);

    impl->cron->async_wait(std::bind(&stats_periodic_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
chamber_t::stats_periodic_action_t::finalize(const boost::system::error_code& ec) {
    if(ec == boost::asio::error::operation_aborted) {
        return;
    }

    struct rusage  this_tick, tick_diff;
    struct timeval real_time = { 0, 0 };

    std::memset(&tick_diff, 0, sizeof(tick_diff));

#if defined(__linux__)
    getrusage(RUSAGE_THREAD, &this_tick);
#else
    getrusage(RUSAGE_SELF, &this_tick);
#endif

    // Calculate the difference.
    timersub(&this_tick.ru_utime, &last_tick.ru_utime, &tick_diff.ru_utime);
    timersub(&this_tick.ru_stime, &last_tick.ru_stime, &tick_diff.ru_stime);

    // Store the snapshot for the next iteration.
    last_tick = this_tick;

    // Sum up the user and system running time.
    timeradd(&tick_diff.ru_utime, &tick_diff.ru_stime, &real_time);

    (*impl->load_average.synchronize())(
        (real_time.tv_sec * 1e6 + real_time.tv_usec) / interval.total_microseconds()
    );

    operator()();
}

// Chamber

namespace bpt = boost::posix_time;

chamber_t::chamber_t(const std::string& name_, const std::shared_ptr<boost::asio::io_service>& asio_):
    name(name_),
    asio(asio_),
    cron(new boost::asio::deadline_timer(*asio)),
    load_average(baf::rolling_window_size = 60 / kCollectionInterval)
{
    asio->post(std::bind(&stats_periodic_action_t::operator(),
        std::make_shared<stats_periodic_action_t>(this, bpt::seconds(kCollectionInterval))
    ));

    // Bootstrap the rolling mean to avoid showing NaNs for the first clients.
    (*load_average.synchronize())(0.0f);

    thread = std::make_unique<boost::thread>(named_runnable_t(name, asio));
}

chamber_t::~chamber_t() {
    cron->cancel();

    // NOTE: This might hang forever if io_service users have failed to abort their async operations
    // upon context.signals.shutdown signal (or haven't connected to it at all).
    thread->join();
}
