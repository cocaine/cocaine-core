#include "cocaine/detail/service/node/app/stats.hpp"

#include <cmath>

#include <boost/accumulators/statistics/extended_p_square.hpp>
#include <boost/accumulators/statistics/extended_p_square_quantile.hpp>

using namespace cocaine;//::detail::service::node::app;

namespace {

static const std::vector<double> probabilities_ = {
    { 0.50, 0.75, 0.90, 0.95, 0.98, 0.99, 0.9995 }
};

inline
double
trunc(double v, uint n) noexcept {
    BOOST_ASSERT(n < 10);

    static const long long table[10] = {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
    };

    return std::ceil(v * table[n]) / table[n];
}

} // namespace

stats_t::stats_t():
    accepted(0),
    rejected(0),
    timings(boost::accumulators::tag::extended_p_square::probabilities = probabilities_)
{}

const std::vector<double>&
stats_t::probabilities() const noexcept {
    return probabilities_;
}

std::vector<stats_t::quantile_t>
stats_t::quantiles() const {
    const auto& probs = this->probabilities();

    std::vector<stats_t::quantile_t> result;
    result.reserve(probs.size());

    timings.apply([&](const quantiles_t& timings) {
        for (std::size_t i = 0; i < probs.size(); ++i) {
            const auto quantile = boost::accumulators::extended_p_square(timings)[i];
            result.push_back({ trunc(100 * probs[i], 2), trunc(quantile, 3) });
        }
    });

    return result;
}
