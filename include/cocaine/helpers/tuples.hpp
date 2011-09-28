#ifndef COCAINE_TUPLES_HPP
#define COCAINE_TUPLES_HPP

#include <boost/mpl/list.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/joint_view.hpp>
#include <boost/mpl/fold.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace helpers {

using namespace boost;

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

template<class T1, class T2>
struct joint_tuple {
    private:
        typedef typename mpl::joint_view<
            typename tuple_type_list<typename T2::inherited>::type,
            typename tuple_type_list<typename T1::inherited>::type
        >::type type_list;

    public:
        typedef typename mpl::fold<
            type_list,
            tuples::null_type,
            tuples::cons<mpl::_2, mpl::_1>
        >::type tuple_type;
};

template<class T1, class T2>
static typename joint_tuple<T1, T2>::tuple_type 
join_tuples(const T1& first, const T2& second) {
    typename joint_tuple<T1, T2>::tuple_type result;

    // Copy!

    return result;
}

}}

#endif
