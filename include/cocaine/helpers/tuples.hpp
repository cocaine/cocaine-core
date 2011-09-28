#ifndef COCAINE_TUPLES_HPP
#define COCAINE_TUPLES_HPP

#include <boost/mpl/list.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/joint_view.hpp>
#include <boost/mpl/lambda.hpp>
#include <boost/mpl/fold.hpp>

#include <boost/type_traits/add_reference.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace helpers {

using namespace boost;

namespace detail {
    template<class TupleType, class List = mpl::list<>::type>
    struct tuple_type_list;
    
    template<class List>
    struct tuple_type_list<tuples::null_type, List> {
        typedef List type;
    };
    
    template<class Head, class Tail, class List>
    struct tuple_type_list<tuples::cons<Head, Tail>, List> {
        typedef typename tuple_type_list<
            Tail,
            typename mpl::push_front<List, Head>::type
        >::type type;
    };
}

template<class T1, class T2>
struct joint_tuple {
    typedef typename mpl::joint_view<
        typename detail::tuple_type_list<typename T2::inherited>::type,
        typename detail::tuple_type_list<typename T1::inherited>::type
    >::type type_list;

    typedef typename mpl::lambda<
        add_reference<mpl::_1>
    >::type reference;

    typedef typename mpl::fold<
        type_list,
        tuples::null_type,
        tuples::cons<
            mpl::bind<reference, mpl::_2>,
            mpl::_1
        >
    >::type type;
};

namespace detail {
    template<size_t Position, class R>
    static void link(R& result, const tuples::null_type&) { }

    template<size_t Position, class R, class T>
    static void link(R& result, const T& source) {
        result.get<Position>() = source.get_head();
        link<Position + 1>(result, source.get_tail());
    }
}

template<class T1, class T2>
static typename joint_tuple<T1, T2>::type
join_tuples(const T1& left, const T2& right) {
    typename joint_tuple<T1, T2>::type result;

    detail::link<0>(result, left);
    detail::link<tuples::length<T1>::value>(result, right);

    return result;
}

}}

#endif
