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

template<typename T>
inline constexpr
const T&
bound(const T& min, const T& value, const T& max) {
    return std::max(min, std::min(value, max));
}

}

void
load_balancer_t::attach(std::shared_ptr<overseer_t> overseer) {
    this->overseer = overseer;
}

slave_t*
load_balancer_t::on_request(const std::string&, const std::string& /*id*/) {
    BOOST_ASSERT(overseer);

    auto pool = overseer->get_pool();

    // If there are no slaves - spawn it.
    if (pool->empty()) {
        overseer->spawn(pool);
        return nullptr;
    }

    // Otherwise find an active slave with minimum load.
    auto it = ::min_element_if(pool->begin(), pool->end(), load(), available {
        overseer->profile.concurrency
    });

    // If all slaves are busy - just delay processing.
    if (it == pool->end()) {
        return nullptr;
    }

    // Otherwise return the slave.
    return &it->second;
}

void
load_balancer_t::on_queue() {
    balance();
}

void
load_balancer_t::on_pool() {
    purge();
}

void
load_balancer_t::purge() {
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

void
load_balancer_t::balance() {
    auto pool = overseer->get_pool();
    auto queue = overseer->get_queue();

    const auto& profile = overseer->profile;

    if (pool->size() >= profile.pool_limit || pool->size() * profile.grow_threshold >= queue->size()) {
        return;
    }

    const auto target = ::bound(1UL, queue->size() / profile.grow_threshold, profile.pool_limit);

    if (target <= pool->size()) {
        return;
    }

    while(pool->size() != target) {
        overseer->spawn(pool);
    }
}
