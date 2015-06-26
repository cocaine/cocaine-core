#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node/slot.hpp"

namespace cocaine {

class control_t;

/// Initial dispatch for slaves.
///
/// Accepts only handshake messages and forwards it to the actual checker (i.e. to the Overseer).
/// This is a single-shot dispatch, it will be invalid after the first handshake processed.
class handshaker_t:
    public dispatch<io::worker_tag>
{
    std::shared_ptr<session_t> session;

    std::mutex mutex;
    std::condition_variable cv;

public:
    template<class F>
    handshaker_t(const std::string& name, F&& fn):
        dispatch<io::worker_tag>(format("%s/handshaker", name))
    {
        typedef io::streaming_slot<io::worker::handshake> slot_type;

        on<io::worker::handshake>(std::make_shared<slot_type>(
            [=](slot_type::upstream_type&& stream, const std::string& uuid) -> std::shared_ptr<control_t>
        {
            std::unique_lock<std::mutex> lock(mutex);
            // TODO: Perhaps we should use here `wait_for` to prevent forever waiting on slave, that
            // have been immediately killed.
            cv.wait(lock, [&]() -> bool {
                return !!session;
            });

            return fn(uuid ,std::move(session), std::move(stream));
        }));
    }

    /// Here we need that shitty const cast, because `io::dispatch_ptr_t` is a shared pointer over
    /// constant dispatch.
    void bind(std::shared_ptr<session_t> session) const {
        const_cast<handshaker_t*>(this)->bind(std::move(session));
    }

    void bind(std::shared_ptr<session_t> session) {
        std::unique_lock<std::mutex> lock(mutex);
        this->session = std::move(session);
        lock.unlock();
        cv.notify_one();
    }
};

}
