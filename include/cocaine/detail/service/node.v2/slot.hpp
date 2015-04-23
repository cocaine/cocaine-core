#pragma once

#include "cocaine/rpc/slot.hpp"

namespace cocaine { namespace io {

template<class T> struct deduce;

template<class Event>
class streaming_slot:
    public io::basic_slot<Event>
{
public:
    typedef Event event_type;

    typedef typename io::basic_slot<event_type>::tuple_type    tuple_type;
    typedef typename io::basic_slot<event_type>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<event_type>::upstream_type upstream_type;

    typedef typename boost::function_types::function_type<
        typename mpl::copy<
            typename io::basic_slot<Event>::sequence_type,
            mpl::back_inserter<
                mpl::vector<
                    std::shared_ptr<const dispatch_type>,
                    typename std::add_rvalue_reference<upstream_type>::type
                >
            >
        >::type
    >::type function_type;

    typedef std::function<function_type> callable_type;

private:
    const callable_type fn;

public:
    streaming_slot(callable_type fn):
        fn(std::move(fn))
    {}

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(tuple_type&& args, upstream_type&& upstream) override {
        return cocaine::tuple::invoke(std::tuple_cat(std::forward_as_tuple(std::move(upstream)), std::move(args)), fn);
    }
};

}}
