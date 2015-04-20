#include "cocaine/detail/service/node.v2/balancing/load.hpp"

using namespace cocaine;

namespace {

template<class It, class Compare, class Predicate>
inline
It
min_element_if(It first, It last, Compare compare, Predicate predicate) {
    while(first != last && !predicate(*first)) {
        ++first;
    }

    if(first == last) {
        return last;
    }

    It result = first;

    while(++first != last) {
        if(predicate(*first) && compare(*first, *result)) {
            result = first;
        }
    }

    return result;
}

struct load {
    template<class T>
    bool
    operator()(const T& lhs, const T& rhs) const {
        return lhs.second.load() < rhs.second.load();
    }
};

struct available {
    template<class T>
    bool
    operator()(const T& slave) const {
        return slave.second.active() && slave.second.load() < max;
    }

    const size_t max;
};

}

void
load_balancer_t::attach(std::shared_ptr<overseer_t> overseer) {
    this->overseer = overseer;
}

slave_t*
load_balancer_t::on_request(const std::string&, const std::string& /*id*/) {
    BOOST_ASSERT(overseer);

    auto pool = overseer->get_pool();

    if (pool->empty()) {
        overseer->spawn(pool);
        return nullptr;
    }

    return nullptr;
}

void
load_balancer_t::on_queue() {
    rebalance();
}

void
load_balancer_t::on_pool() {
    rebalance();
}

void
load_balancer_t::rebalance() {
    BOOST_ASSERT(overseer);

    auto pool = overseer->get_pool();
    if (pool->empty()) {
        return;
    }

    auto queue = overseer->get_queue();

    while (!queue->empty()) {
        auto it = ::min_element_if(pool->begin(), pool->end(), load(), available {
            overseer->profile.concurrency
        });

        if(it == pool->end()) {
            return;
        }

        auto payload = std::move(queue->front());
        queue->pop();

        overseer->assign(it->second, payload);
    }
}
