#ifndef COCAINE_TUPLES_HPP
#define COCAINE_TUPLES_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace helpers {

using namespace boost;
using namespace boost::tuples;

namespace detail {
    template<class LT, class RT>
    struct chain {
        typedef typename add_reference<typename LT::head_type>::type head_type;
        typedef chain<typename LT::tail_type, RT> chain_type;
        typedef cons<head_type, typename chain_type::type> type;

        static type apply(LT& left, RT& right) {
            return type(left.get_head(), chain_type::apply(left.get_tail(), right));
        }
    };
    
    template<class RT>
    struct chain<null_type, RT> {
        typedef typename add_reference<typename RT::head_type>::type head_type;
        typedef chain<typename RT::tail_type, null_type> chain_type;
        typedef cons<head_type, typename chain_type::type> type;
    
        static type apply(null_type left, RT& right) {
            return type(right.get_head(), chain_type::apply(right.get_tail(), left));
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
