/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_DYNAMIC_TYPE_HPP
#define COCAINE_DYNAMIC_TYPE_HPP

#include "cocaine/errors.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/utility.hpp"

#include <string>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <boost/variant.hpp>

#include "cocaine/dynamic/detail.hpp"

namespace cocaine {

template<class From, class = void>
struct dynamic_constructor {
    static const bool enable = false;
};

template<class To, class = void>
struct dynamic_converter { };

class dynamic_t
{
public:
    typedef bool                   bool_t;
    typedef int64_t                int_t;
    typedef uint64_t               uint_t;
    typedef double                 double_t;
    typedef std::string            string_t;
    typedef std::vector<dynamic_t> array_t;

    struct null_t {
        bool
        operator==(const null_t&) const {
            return true;
        }
    };

    class object_t;

    typedef boost::variant<
        null_t,
        bool_t,
        int_t,
        uint_t,
        double_t,
        string_t,
        detail::dynamic::incomplete_wrapper<array_t>,
        detail::dynamic::incomplete_wrapper<object_t>
    > value_t;

    // Just useful constants which may be accessed by reference from any place of the program.
    static const dynamic_t null;
    static const dynamic_t empty_string;
    static const dynamic_t empty_array;
    static const dynamic_t empty_object;

public:
    dynamic_t();
    dynamic_t(const dynamic_t& other);
    dynamic_t(dynamic_t&& other);

    template<class T>
    dynamic_t(
        T&& from,
        typename std::enable_if<dynamic_constructor<typename pristine<T>::type>::enable>::type* = 0
    );

    dynamic_t&
    operator=(const dynamic_t& other);

    dynamic_t&
    operator=(dynamic_t&& other);

    template<class T>
    typename std::enable_if<dynamic_constructor<typename pristine<T>::type>::enable, dynamic_t&>::type
    operator=(T&& from);

    bool
    operator==(const dynamic_t& other) const;

    bool
    operator!=(const dynamic_t& other) const;

    template<class Visitor>
    typename Visitor::result_type
    apply(const Visitor& visitor) {
        return boost::apply_visitor(
            detail::dynamic::dynamic_visitor_applier<const Visitor&, typename Visitor::result_type>(visitor),
            m_value
        );
    }

    template<class Visitor>
    typename Visitor::result_type
    apply(Visitor& visitor) {
        return boost::apply_visitor(
            detail::dynamic::dynamic_visitor_applier<Visitor&, typename Visitor::result_type>(visitor),
            m_value
        );
    }

    template<class Visitor>
    typename Visitor::result_type
    apply(const Visitor& visitor) const {
        return boost::apply_visitor(
            detail::dynamic::const_visitor_applier<const Visitor&, typename Visitor::result_type>(visitor),
            m_value
        );
    }

    template<class Visitor>
    typename Visitor::result_type
    apply(Visitor& visitor) const {
        return boost::apply_visitor(
            detail::dynamic::const_visitor_applier<Visitor&, typename Visitor::result_type>(visitor),
            m_value
        );
    }

    bool
    is_null() const;

    bool
    is_bool() const;

    bool
    is_int() const;

    bool
    is_uint() const;

    bool
    is_double() const;

    bool
    is_string() const;

    bool
    is_array() const;

    bool
    is_object() const;

    bool_t
    as_bool() const;

    int_t
    as_int() const;

    uint_t
    as_uint() const;

    double_t
    as_double() const;

    const string_t&
    as_string() const;

    const array_t&
    as_array() const;

    const object_t&
    as_object() const;

    string_t&
    as_string();

    array_t&
    as_array();

    object_t&
    as_object();

    template<class T>
    bool
    convertible_to() const;

    template<class T>
    typename dynamic_converter<typename pristine<T>::type>::result_type
    to() const;

private:
    template<class T>
    T&
    get() {
        try {
            return boost::get<T>(m_value);
        } catch (const boost::bad_get& e) {
            throw error_t("failed to get node value as {} - got {}", logging::demangle<T>(), boost::lexical_cast<std::string>(*this));
        }
    }

    template<class T>
    const T&
    get() const {
        return const_cast<dynamic_t*>(this)->get<T>();
    }

    template<class T>
    bool
    is() const {
        return static_cast<bool>(boost::get<T>(&m_value));
    }

private:
    // boost::apply_visitor takes non-constant reference to a variant object.
    value_t mutable m_value;
};

template<class T>
dynamic_t::dynamic_t(
    T&& from,
    typename std::enable_if<dynamic_constructor<typename pristine<T>::type>::enable>::type*)
:
    m_value(null_t())
{
    dynamic_constructor<typename pristine<T>::type>::convert(std::forward<T>(from), m_value);
}

template<class T>
typename std::enable_if<dynamic_constructor<typename pristine<T>::type>::enable, dynamic_t&>::type
dynamic_t::operator=(T&& from) {
    dynamic_constructor<typename pristine<T>::type>::convert(std::forward<T>(from), m_value);
    return *this;
}

template<class T>
bool
dynamic_t::convertible_to() const {
    return dynamic_converter<typename pristine<T>::type>::convertible(*this);
}

template<class T>
typename dynamic_converter<typename pristine<T>::type>::result_type
dynamic_t::to() const {
    return dynamic_converter<typename pristine<T>::type>::convert(*this);
}

} // namespace cocaine

#include "cocaine/dynamic/object.hpp"
#include "cocaine/dynamic/constructors.hpp"
#include "cocaine/dynamic/converters.hpp"

namespace boost {

template<>
std::string
lexical_cast<std::string, cocaine::dynamic_t>(const cocaine::dynamic_t&);

} // namespace boost

#endif // COCAINE_DYNAMIC_TYPE_HPP
