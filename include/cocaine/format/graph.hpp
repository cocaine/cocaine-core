#pragma once

#include "cocaine/format/base.hpp"
#include "cocaine/format/map.hpp"
#include "cocaine/format/optional.hpp"
#include "cocaine/format/tuple.hpp"
#include "cocaine/rpc/graph.hpp"

namespace cocaine {

template<>
struct display<io::graph_node_t> : public display<io::aux::recursion_base_t> {};

template<>
struct display_traits<io::graph_node_t> : public display_traits<io::aux::recursion_base_t> {};

} // namespace cocaine
