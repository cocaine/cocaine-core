#include "cocaine/detail/service/node/balancing/load.hpp"

#include "cocaine/detail/service/node/overseer.hpp"

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
    operator()(const T& it) const {
        return it.second.active() && it.second.load() < max;
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

load_balancer_t::load_balancer_t(std::shared_ptr<overseer_t> overseer):
    balancer_t(std::move(overseer))
{}

slave_info
load_balancer_t::on_request(const std::string&, const std::string& /*id*/) {
    BOOST_ASSERT(overseer);

    auto pool = overseer->get_pool();

    // If there are no slaves - spawn it.
    if (pool->empty()) {
        overseer->spawn(pool);
        return slave_info();
    }

    // Otherwise find an active slave with minimum load.
    auto it = ::min_element_if(pool->begin(), pool->end(), load(), available {
        overseer->profile().concurrency
    });

    // If all slaves are busy - just delay processing.
    if (it == pool->end()) {
        return slave_info();
    }

    // Otherwise return the slave.
    return slave_info { &it->second };
}

void
load_balancer_t::on_slave_spawn(const std::string& /*uuid*/) {
    COCAINE_LOG_TRACE(overseer->logger(), "slave has been added to balancer");
    purge();
}

void
load_balancer_t::on_slave_death(const std::string& /*uuid*/) {
    COCAINE_LOG_TRACE(overseer->logger(), "slave has been removed from balancer");
    balance();
}

void
load_balancer_t::on_queue() {
    purge();
    balance();
}

void
load_balancer_t::on_channel_started(const std::string& /*uuid*/, std::uint64_t /*channel*/) {
}

void
load_balancer_t::on_channel_finished(const std::string& /*uuid*/, std::uint64_t /*channel*/) {
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
            overseer->profile().concurrency
        });

        if(it == pool->end()) {
            return;
        }

        auto& payload = queue->front();

        try {
            overseer->assign(it->second, payload);
            // The slave may become invalid and reject the assignment (or reject for any other
            // reasons). We pop the channel only on successful assignment to achieve strong
            // exception guarantee.
            queue->pop();
        } catch (const std::exception& err) {
            COCAINE_LOG_DEBUG(overseer->logger(), "slave has rejected assignment: %s", err.what());
        }
    }
}

void
load_balancer_t::balance() {
    const auto queue_size = overseer->get_queue()->size();

    const auto profile = overseer->profile();

    auto pool = overseer->get_pool();
    if (pool->size() >= profile.pool_limit || pool->size() * profile.grow_threshold >= queue_size) {
        return;
    }

    const auto target = ::bound(1UL, queue_size / profile.grow_threshold, profile.pool_limit);

    if (target <= pool->size()) {
        return;
    }

    while(pool->size() < target) {
        overseer->spawn(pool);
    }
}
