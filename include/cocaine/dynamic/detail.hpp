/*
    Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_DYNAMIC_DETAIL_HPP
#define COCAINE_DYNAMIC_DETAIL_HPP

#include <map>
#include <memory>

namespace cocaine {

class dynamic_t;

namespace detail { namespace dynamic {

// Helps to use STL containers
template<class T>
class incomplete_wrapper {
public:
    // These *structors are needed just to satisfy the requirements of boost::variant.
    incomplete_wrapper() {
        // Empty.
    }

    incomplete_wrapper(const incomplete_wrapper&) {
        // Empty.
    }

    incomplete_wrapper&
    operator=(const incomplete_wrapper&) {
        return *this;
    }

    T&
    get() {
        return *m_data;
    }

    const T&
    get() const {
        return *m_data;
    }

    template<class... Args>
    void
    set(Args&&... args) {
        m_data.reset(new T(std::forward<Args>(args)...));
    }

private:
    std::unique_ptr<T> m_data;
};

template<class Visitor, class Result>
struct dynamic_visitor_applier :
    public boost::static_visitor<Result>
{
    dynamic_visitor_applier(Visitor v) :
        m_visitor(v)
    { }

    template<class T>
    Result
    operator()(T& v) const {
        return m_visitor(static_cast<const T&>(v));
    }

    template<class T>
    Result
    operator()(incomplete_wrapper<T>& v) const {
        return m_visitor(v.get());
    }

private:
    Visitor m_visitor;
};

template<class ConstVisitor, class Result>
struct const_visitor_applier :
    public boost::static_visitor<Result>
{
    const_visitor_applier(ConstVisitor v) :
        m_const_visitor(v)
    { }

    template<class T>
    Result
    operator()(T& v) const {
        return m_const_visitor(static_cast<const T&>(v));
    }

    template<class T>
    Result
    operator()(incomplete_wrapper<T>& v) const {
        return m_const_visitor(static_cast<const T&>(v.get()));
    }

private:
    ConstVisitor m_const_visitor;
};

}}} // namespace cocaine::detail::dynamic

#endif // COCAINE_DYNAMIC_DETAIL_HPP
