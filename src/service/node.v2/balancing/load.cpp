#include "cocaine/detail/service/node.v2/balancing/load.hpp"

using namespace cocaine;

void
load_balancer_t::attach(std::shared_ptr<overseer_t> overseer) {
    this->overseer = overseer;
}

std::shared_ptr<streaming_dispatch_t>
load_balancer_t::queue_changed(io::streaming_slot<io::app::enqueue>::upstream_type&, std::string) {
    BOOST_ASSERT(overseer);

//        auto info = overseer->info();

    // TODO: Insert normal spawn condition here.
//        if (info.pool == 0) {
//            overseer->spawn();
//            return nullptr;
//        }

//        auto pool = overseer->get_pool();

//        // TODO: Get slave with minimum load.
//        auto slave = pool->begin()->second;
//        if (auto slave_ = boost::get<std::shared_ptr<slave::active_t>>(&slave)) {
//            auto cwu = (*slave_)->inject(std::make_shared<worker_client_dispatch_t>(wcu));
//            cwu->send<io::worker::rpc::invoke>(event);

//            auto dispatch = std::make_shared<streaming_dispatch_t>("c->w");
//            dispatch->attach(std::make_shared<upstream<io::event_traits<io::worker::rpc::invoke>::dispatch_type>>(cwu));
//            return dispatch;
//        } else {
//            return nullptr;
//        }
    return nullptr;
}

void
load_balancer_t::pool_changed() {
    BOOST_ASSERT(overseer);

//        {
//            auto queue = overseer->get_queue();
//            if (queue->empty()) {
//                return;
//            }
//        }

//        auto payload = [&]() -> overseer_t::queue_value {
//            auto queue = overseer->get_queue();

//            auto payload = queue->front();
//            queue->pop();
//            return payload;
//        }();

//        auto pool = overseer->get_pool();

//        auto slave = pool->begin()->second;
//        if (auto slave_ = boost::get<std::shared_ptr<slave::active_t>>(&slave)) {
//            auto cwu = (*slave_)->inject(std::make_shared<worker_client_dispatch_t>(payload.upstream));
//            cwu->send<io::worker::rpc::invoke>(payload.event);

//            auto dispatch = payload.dispatch;
//            dispatch->attach(std::make_shared<upstream<io::event_traits<io::worker::rpc::invoke>::dispatch_type>>(cwu));
//        }
}
