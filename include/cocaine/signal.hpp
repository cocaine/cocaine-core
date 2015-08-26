/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
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


#ifndef COCAINE_SIGNAL_SET_HPP
#define COCAINE_SIGNAL_SET_HPP

#include "cocaine/locked_ptr.hpp"
#include "cocaine/forwards.hpp"

#include <atomic>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <signal.h>

namespace cocaine { namespace signal {

class cancellation_t {
public:
    // Call cancellation callback if present, was not called before and was not detached.
    // Return true if callback was called
    bool
    cancel();

    // Detach cancellation token so the destructor or cancel() would not call cancellation callback.
    // Usually used right after async wait, f.e. signal_handler.async_wait(cb).detach();
    // Return if callback was sucessfully detached(was not cancelled before, detached before and callback is present)
    bool
    detach();

    cancellation_t();
    cancellation_t(std::function<bool()> cancellation_callback);
    cancellation_t(const cancellation_t&) = delete;
    cancellation_t& operator=(const cancellation_t&) = delete;
    cancellation_t(cancellation_t&&);
    cancellation_t& operator=(cancellation_t&&);
    ~cancellation_t();

private:
    std::atomic_flag cancelled;
    std::function<bool()> cancellation_callback;
};

class handler_base_t {

public:
    typedef std::function<void(const std::error_code&, int)> simple_callback_type;
    typedef std::function<void(const std::error_code&, int, const siginfo_t&)> detailed_callback_type;

    virtual
    ~handler_base_t() {}

    // Workaround for stupid gcc 4.6 which cannot choose overload
    // if we pass lambda.
    template<class Callable>
    typename std::enable_if<
        std::is_void<
            decltype(std::declval<Callable>()(
                         std::declval<const std::error_code&>(),
                         std::declval<int>(),
                         std::declval<const siginfo_t&>()))
        >::value, cancellation_t
    >::type
    async_wait(int signum, Callable handler) {
        return async_wait_detailed(signum, detailed_callback_type(std::move(handler)));
    }

    template<class Callable>
    typename std::enable_if<
        std::is_void<
            decltype(std::declval<Callable>()(
                 std::declval<const std::error_code&>(),
                 std::declval<int>()))
        >::value, cancellation_t
    >::type
    async_wait(int signum, Callable handler) {
        return async_wait_simple(signum, simple_callback_type(std::move(handler)));
    }

private:
    // Setup a single time handler for specified signal number.
    // It should be reset up on signal.
    // If you want to make recurrent one call async_wait inside handler in libasio manner
    virtual cancellation_t
    async_wait_simple(int signum, simple_callback_type handler) = 0;

    // As the previous one but also pass siginfo_t to handler
    virtual cancellation_t
    async_wait_detailed(int signum, detailed_callback_type handler) = 0;
};

class handler_t : public handler_base_t {

public:
    typedef handler_base_t::simple_callback_type simple_callback_type;
    typedef handler_base_t::detailed_callback_type detailed_callback_type;

    typedef std::map<uint64_t, detailed_callback_type> detailed_callback_storage;

    handler_t(std::unique_ptr<cocaine::logging::logger_t> logger, std::set<int> signal_set);

    handler_t(const handler_t&) = delete;
    handler_t(handler_t&&) = delete;
    handler_t& operator=(const handler_t&) = delete;
    handler_t& operator=(handler_t&&) = delete;

    virtual
    ~handler_t();

    void
    run();

    void
    stop();

private:
    // Setup a single time handler for specified signal number.
    // It should be reset up on signal.
    virtual cancellation_t
    async_wait_simple(int signum, simple_callback_type handler);

    // Setup a single time handler for specified signal number.
    // Siginfo is passed in this overload.
    // It should be reset up on signal.
    virtual cancellation_t
    async_wait_detailed(int signum, detailed_callback_type handler);

    std::unique_ptr<cocaine::logging::logger_t> logger;

    std::set<int> signals;

    // each entry is signal's(determined by index) vector of handlers.
    synchronized<std::vector<detailed_callback_storage>> registered_signal_callbacks;

    std::mutex process_lock;
    bool should_run;
};

}}
#endif // COCAINE_SIGNAL_SET_HPP

