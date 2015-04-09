#pragma once

#include <boost/variant/variant.hpp>

namespace cocaine {

template<class R, class... Other>
class lambda_visitor;

template<class R, class L, typename... Other>
class lambda_visitor<R, L, Other...> :
    public lambda_visitor<R, Other...>,
    public L
{
public:
    using L::operator();
    using lambda_visitor<R, Other...>::operator();

    lambda_visitor(L fn, Other... other) :
        lambda_visitor<R, Other...>(other...),
        L(std::move(fn))
    {}
};

template<class R, class L>
class lambda_visitor<R, L> :
    public boost::static_visitor<R>,
    public L
{
public:
    using L::operator();

    lambda_visitor(L fn) :
        boost::static_visitor<R>(),
        L(std::move(fn))
    {}
};

template<class R>
class lambda_visitor<R> :
    public boost::static_visitor<R>
{
public:
    lambda_visitor() : boost::static_visitor<R>() {}
};

template<typename R, typename T, class... Lambda>
inline
auto
match(T&& variant, Lambda&&... lambda) -> R {
    return boost::apply_visitor(lambda_visitor<R, Lambda...>(std::forward<Lambda>(lambda)...),
                                std::forward<T>(variant));
}

template<typename T, class... Lambda>
inline
auto
match(T&& variant, Lambda&&... lambda) -> void {
    return match<void>(std::forward<T>(variant), std::forward<Lambda>(lambda)...);
}

}
