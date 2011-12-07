//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef COCAINE_NETWORKING_TUPLES_HPP
#define COCAINE_NETWORKING_TUPLES_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace helpers {

using namespace boost::tuples;

namespace {
    template<class Current, class Next>
    struct chain {
        typedef chain<typename Current::tail_type, Next> chain_type;
        typedef cons<typename Current::head_type, typename chain_type::type> type;

        static type apply(const Current& current, const Next& next) {
            return type(current.get_head(), chain_type::apply(current.get_tail(), next));
        }
    };
    
    template<class Next>
    struct chain<null_type, Next> {
        typedef chain<typename Next::tail_type, null_type> chain_type;
        typedef cons<typename Next::head_type, typename chain_type::type> type;
    
        static type apply(null_type null, const Next& next) {
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
static typename chain<LT, RT>::type
inline joint_view(const LT& lt, const RT& rt) {
    return chain<LT, RT>::apply(lt, rt);
}

}}

#endif
