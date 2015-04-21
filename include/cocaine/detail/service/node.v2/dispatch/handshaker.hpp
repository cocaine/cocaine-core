#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/service/node.v2/slot.hpp"

namespace cocaine {

class control_t;

/// Initial dispatch for slaves.
///
/// Accepts only handshake messages and forwards it to the actual checker (i.e. to the Overseer).
/// This is a single-shot dispatch, it will be invalid after the first handshake processed.
class handshaker_t:
    public dispatch<io::worker_tag>
{
    mutable std::shared_ptr<session_t> session;
    mutable std::mutex mutex;
    mutable std::condition_variable cv;

public:
    template<class F>
    handshaker_t(const std::string& name, F&& fn):
        dispatch<io::worker_tag>(format("%s/handshaker", name))
    {
        typedef io::streaming_slot<io::worker::handshake> slot_type;

        on<io::worker::handshake>(std::make_shared<slot_type>(
            [=](slot_type::upstream_type& stream, const std::string& uuid) -> std::shared_ptr<control_t>
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!session) {
                cv.wait(lock);
            }

            return fn(stream, uuid, std::move(session));
        }));
    }

    /// Here we need mutable variables, because io::dispatch_ptr_t is a shared pointer over constant
    /// dispatch.
    void bind(std::shared_ptr<session_t> session) const {
        std::unique_lock<std::mutex> lock(mutex);
        this->session = std::move(session);
        lock.unlock();
        cv.notify_one();
    }
};

}
