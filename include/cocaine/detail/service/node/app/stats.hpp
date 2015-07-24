#pragma once

#include <atomic>
#include <cstdint>

#include <boost/accumulators/framework/accumulator_set.hpp>
#include <boost/accumulators/statistics.hpp>

#include "cocaine/locked_ptr.hpp"

namespace cocaine { // namespace detail { namespace service { namespace node { namespace app {

struct stats_t {
    struct {
        /// The number of requests, that are pushed into the queue.
        std::atomic<std::uint64_t> accepted;

        /// The number of requests, that were rejected due to queue overflow or other circumstances.
        std::atomic<std::uint64_t> rejected;
    } requests;

    struct {
        /// The number of successfully spawned slaves.
        std::atomic<std::uint64_t> spawned;

        /// The number of crashed slaves.
        std::atomic<std::uint64_t> crashed;
    } slaves;

    /// Channel processing time quantiles (summary).
    typedef boost::accumulators::accumulator_set<
        double,
        boost::accumulators::stats<
            boost::accumulators::tag::extended_p_square
        >
    > quantiles_t;

    synchronized<quantiles_t> timings;

    stats_t();

    struct quantile_t {
        double probability;
        double value;
    };

    std::vector<quantile_t>
    quantiles() const;

private:
    const std::vector<double>&
    probabilities() const noexcept;
};

} //}}}} // namespace cocaine::detail::service::node::app
