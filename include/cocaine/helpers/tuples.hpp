#ifndef COCAINE_TUPLES_HPP
#define COCAINE_TUPLES_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace helpers {

using namespace boost;
using namespace boost::tuples;

namespace detail {
    template<class Current, class Next>
    struct chain {
        typedef typename add_reference<typename Current::head_type>::type head_type;
        typedef chain<typename Current::tail_type, Next> chain_type;
        typedef cons<head_type, typename chain_type::type> type;

        static type apply(Current& current, Next& next) {
            return type(current.get_head(), chain_type::apply(current.get_tail(), next));
        }
    };
    
    template<class Next>
    struct chain<null_type, Next> {
        typedef typename add_reference<typename Next::head_type>::type head_type;
        typedef chain<typename Next::tail_type, null_type> chain_type;
        typedef cons<head_type, typename chain_type::type> type;
    
        static type apply(null_type null, Next& next) {
            return type(next.get_head(), chain_type::apply(next.get_tail(), null));
        }
    };
    
    template<>
    struct chain<null_type, null_type> {
        typedef null_type type;
        static null_type null;

        static type apply(null_type, null_type) {
            return null;
        }
    };
}

template<class LT, class RT>
static typename detail::chain<LT, RT>::type
joint_view(LT& lt, RT& rt) {
    return detail::chain<LT, RT>::apply(lt, rt);
}

}}

#endif
