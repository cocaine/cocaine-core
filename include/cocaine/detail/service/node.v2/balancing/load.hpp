#pragma once

#include "cocaine/detail/service/node.v2/app.hpp"

namespace cocaine {

class load_balancer_t:
    public balancer_t
{
    std::shared_ptr<overseer_t> overseer;

public:
    void
    attach(std::shared_ptr<overseer_t> overseer);

    virtual
    slave_t*
    on_request(const std::string& event, const std::string& id);

    virtual
    void
    on_queue();

    void
    on_pool();

private:
    void
    rebalance();
};

}
