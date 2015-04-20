#pragma once

#include "cocaine/detail/service/node.v2/app.hpp"

namespace cocaine {

class load_balancer_t : public balancer_t {
    std::shared_ptr<overseer_t> overseer;

public:
    void
    attach(std::shared_ptr<overseer_t> overseer);

    virtual
    std::shared_ptr<streaming_dispatch_t>
    queue_changed(io::streaming_slot<io::app::enqueue>::upstream_type& /*wcu*/, std::string /*event*/);

    void
    pool_changed();
};

}
