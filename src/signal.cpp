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

#include "cocaine/format.hpp"
#include "cocaine/signal.hpp"
#include "cocaine/logging.hpp"

#include <blackhole/logger.hpp>

#include <boost/assert.hpp>

#include <cstdlib>
#include <iostream>
#include <fcntl.h>

namespace {
int wait_wrapper(sigset_t* set, siginfo_t* info) {
#ifdef __linux__
    return ::sigwaitinfo(set, info);
#else
    // No better solution for Mac for now.
    *info = siginfo_t();
    int ret;
    int ec = ::sigwait(set, &ret);
    if(ec != 0) {
        errno = ec;
        return -1;
    }
    return ret;
#endif
}

bool pending() {
    sigset_t set;
    int rc = sigpending(&set);
    // sigpending according to docs only produces E_FAULT which should never happen.
    BOOST_ASSERT(rc == 0);
#if defined(BOOST_DISABLE_ASSERTS)
    ((void)rc);
#endif
#ifdef  __GLIBC__
    return sigisemptyset(&set);
#else
    sigset_t empty_set;
    sigemptyset(&empty_set);
    //last hope to compare as usual POD type.
    return set != empty_set;
#endif
}
int interrupt_signal = SIGCONT;

void
stub_handler(int) {
}

sigset_t
get_signal_set(const std::set<int>& signals) {
    sigset_t sigset = sigset_t();
    for(int sig : signals) {
        if(sig == SIGSEGV || sig == SIGFPE || sig == SIGILL || sig == SIGBUS) {
            throw std::system_error(std::make_error_code(std::errc::invalid_argument), "can not handle trap signal in sighandler");
        }
        sigaddset(&sigset, sig);
    }
    return sigset;
}

void
default_action(int) {
    BOOST_ASSERT_MSG(false, "default actions are not supported");
}

}

namespace cocaine { namespace signal {

namespace {
std::atomic_flag initialized;

std::atomic<uint64_t> current_index;
}

bool
cancellation_t::cancel() {
    if(!cancelled.test_and_set()) {
        return cancellation_callback();
    }
    return false;
}

bool
cancellation_t::detach() {
    return !cancelled.test_and_set();
}

cancellation_t::cancellation_t() {
    cancelled.test_and_set();
}

cancellation_t::cancellation_t(std::function<bool()> _cancellation_callback) :
    cancelled(),
    cancellation_callback(std::move(_cancellation_callback))
{
}


cancellation_t::cancellation_t(cancellation_t&& other):
    cancelled(),
    cancellation_callback()
{
    // Constructor from rvalue. Should be safe
    if(other.cancelled.test_and_set()) {
        cancelled.test_and_set();
    } else {
        cancellation_callback = std::move(other.cancellation_callback);
    }
}

cancellation_t&
cancellation_t::operator=(cancellation_t&& other) {
    if(this == &other) {
        return *this;
    }
    cancel();
    if(!other.cancelled.test_and_set()) {
        cancellation_callback = std::move(other.cancellation_callback);
        cancelled.clear();
    }
    return *this;
}

cancellation_t::~cancellation_t() {
    detach();
}

handler_t::handler_t(std::unique_ptr<cocaine::logging::logger_t> logger_, std::set<int> signals_) :
    logger(std::move(logger_)),
    signals(std::move(signals_))
{
    signals.insert(interrupt_signal);
    // Use this instead of singletone to control lifetime better.
    if(initialized.test_and_set()) {
        BOOST_ASSERT_MSG(false, "can not instantiate 2 or more signal handlers at the same time");
        _exit(1);
    }

    sigset_t sigset = get_signal_set(signals);
    ::sigprocmask(SIG_BLOCK, &sigset, nullptr);

    struct sigaction sa;
    sa.sa_handler = stub_handler;
    for(int sig : signals) {
        sigaction(sig, &sa, nullptr);
    }

}

handler_t::~handler_t() {
    sigset_t sigset = get_signal_set(signals);
    ::sigprocmask(SIG_UNBLOCK, &sigset, nullptr);
}

void
handler_t::stop() {
    should_run = false;
    raise(interrupt_signal);
}

void
handler_t::run() {
    siginfo_t info;
    should_run = true;
    sigset_t sig_set = get_signal_set(signals);
    while(should_run || pending()) {
        const int signal_num = wait_wrapper(&sig_set, &info);
        COCAINE_LOG_DEBUG(logger, "caught signal: {}", signal_num);
        if(signal_num == -1) {
            throw std::system_error(std::error_code(errno, std::system_category()), "sigwaitinfo failed");
        }
        // Do not process interruption signal in case stop() was received.
        // As we use SIGCONT there is no harm of doing this
        if(signal_num == interrupt_signal && !should_run) {
            continue;
        }
        // We need a storage to move all callbacks at one hop to prevent deadlock while executing handlers which reset themselves.
        detailed_callback_storage tmp_storage;
        std::lock_guard<std::mutex> guard(process_lock);
        registered_signal_callbacks.apply([&](std::vector<detailed_callback_storage>& callbacks) {
            if(callbacks.size() > static_cast<size_t>(signal_num)) {
                // Move callbacks at one hop and process them outside lock
                tmp_storage = std::move(callbacks[signal_num]);
            }

        });
        if(tmp_storage.empty()) {
            COCAINE_LOG_DEBUG(logger, "running default action for signal: {}", signal_num);
            default_action(signal_num);
        }
        for(auto& cb_pair : tmp_storage) {
            COCAINE_LOG_DEBUG(logger, "running handler with index {} for signal: {}", cb_pair.first, signal_num);
            cb_pair.second(std::error_code(), signal_num, info);
        }
    }

    std::vector<detailed_callback_storage> callbacks;
    registered_signal_callbacks.apply([&](std::vector<detailed_callback_storage>& _callbacks){
        callbacks = std::move(_callbacks);
    });

    for(auto& cb_vector: callbacks) {
        for(auto& cb: cb_vector) {
            auto ec = std::make_error_code(std::errc::operation_canceled);
            siginfo_t info = siginfo_t();
            cb.second(ec, 0, info);
        }
    }
}

cancellation_t
handler_t::async_wait_detailed(int signum, detailed_callback_type handler) {
    cancellation_t token;
    registered_signal_callbacks.apply([&](std::vector<detailed_callback_storage>& callbacks) {
        if(callbacks.size() <= static_cast<size_t>(signum)) {
            callbacks.resize(signum + 1);
        }
        auto index = current_index++;
        callbacks[signum][index] = handler;
        token = cancellation_t([=]() -> bool {
            return registered_signal_callbacks.apply([&](std::vector<detailed_callback_storage>& callbacks) -> bool {
                auto cb_it = callbacks[signum].find(index);
                bool result = true;
                if(cb_it != callbacks[signum].end()) {
                    cb_it->second(std::make_error_code(std::errc::operation_canceled), 0, siginfo_t());
                    callbacks[signum].erase(cb_it);
                } else {
                    result = false;
                    // In this specific situation
                    // Wait until current signal is processed(if there is one) to be sure
                    // That we are not processing this callback right now.
                    std::lock_guard<std::mutex> guard(process_lock);
                }
                return result;
            });
        });
    });
    return token;
}

cancellation_t
handler_t::async_wait_simple(int signum, simple_callback_type handler) {
    return async_wait(signum, [=](const std::error_code& ec, int signum, const siginfo_t&) {
       handler(ec, signum);
    });
}

}} //namespace cocaine::signal

