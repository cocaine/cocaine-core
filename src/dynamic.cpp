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

#include <cocaine/dynamic/dynamic.hpp>

using namespace cocaine;

const dynamic_t dynamic_t::null = dynamic_t::null_t();
const dynamic_t dynamic_t::empty_string = dynamic_t::string_t();
const dynamic_t dynamic_t::empty_array = dynamic_t::array_t();
const dynamic_t dynamic_t::empty_object = dynamic_t::object_t();

cocaine::dynamic_t&
dynamic_t::object_t::at(const std::string& key, cocaine::dynamic_t& default_) {
    auto it = find(key);

    if(it == end()) {
        return default_;
    } else {
        return it->second;
    }
}

const cocaine::dynamic_t&
dynamic_t::object_t::at(const std::string& key, const cocaine::dynamic_t& default_) const {
    auto it = find(key);

    if(it == end()) {
        return default_;
    } else {
        return it->second;
    }
}

const cocaine::dynamic_t&
dynamic_t::object_t::operator[](const std::string& key) const {
    return at(key);
}

struct move_visitor:
    public boost::static_visitor<>
{
    move_visitor(dynamic_t& destination):
        m_destination(destination)
    { }

    template<class T>
    void
    operator()(T& v) const {
        m_destination = std::move(v);
    }

private:
    dynamic_t& m_destination;
};

struct assign_visitor:
    public boost::static_visitor<>
{
    assign_visitor(dynamic_t& destination):
        m_destination(destination)
    { }

    template<class T>
    void
    operator()(T& v) const {
        m_destination = v;
    }

private:
    dynamic_t& m_destination;
};

struct equals_visitor:
    public boost::static_visitor<bool>
{
    equals_visitor(const dynamic_t& other) :
        m_other(other)
    { }

    bool
    operator()(const dynamic_t::null_t&) const {
        return m_other.is_null();
    }

    bool
    operator()(const dynamic_t::bool_t& v) const {
        return m_other.is_bool() && m_other.as_bool() == v;
    }

    bool
    operator()(const dynamic_t::int_t& v) const {
        if(m_other.is_int()) {
            return m_other.as_int();
        } else {
            return m_other.is_uint() && v >= 0 && static_cast<dynamic_t::uint_t>(v) == m_other.as_uint();
        }
    }

    bool
    operator()(const dynamic_t::uint_t& v) const {
        if(m_other.is_uint()) {
            return m_other.as_uint();
        } else {
            return m_other.is_int() && m_other.as_int() >= 0 && v == m_other.to<dynamic_t::uint_t>();
        }
    }

    bool
    operator()(const dynamic_t::double_t& v) const {
        return m_other.is_double() && m_other.as_double() == v;
    }

    bool
    operator()(const dynamic_t::string_t& v) const {
        return m_other.is_string() && m_other.as_string() == v;
    }

    bool
    operator()(const dynamic_t::array_t& v) const {
        return m_other.is_array() && m_other.as_array() == v;
    }

    bool
    operator()(const dynamic_t::object_t& v) const {
        return m_other.is_object() && m_other.as_object() == v;
    }

private:
    const dynamic_t& m_other;
};

dynamic_t::dynamic_t() :
    m_value(null_t())
{ }

dynamic_t::dynamic_t(const dynamic_t& other) :
    m_value(null_t())
{
    other.apply(assign_visitor(*this));
}

dynamic_t::dynamic_t(dynamic_t&& other) :
    m_value(null_t())
{
    other.apply(move_visitor(*this));
}

dynamic_t&
dynamic_t::operator=(const dynamic_t& other) {
    other.apply(assign_visitor(*this));
    return *this;
}

dynamic_t&
dynamic_t::operator=(dynamic_t&& other) {
    other.apply(move_visitor(*this));
    return *this;
}

bool
dynamic_t::operator==(const dynamic_t& other) const {
    return other.apply(equals_visitor(*this));
}

bool
dynamic_t::operator!=(const dynamic_t& other) const {
    return !other.apply(equals_visitor(*this));
}

dynamic_t::bool_t
dynamic_t::as_bool() const {
    return get<bool_t>();
}

dynamic_t::int_t
dynamic_t::as_int() const {
    return get<int_t>();
}

dynamic_t::uint_t
dynamic_t::as_uint() const {
    return get<uint_t>();
}

dynamic_t::double_t
dynamic_t::as_double() const {
    return get<double_t>();
}

const dynamic_t::string_t&
dynamic_t::as_string() const {
    return get<string_t>();
}

const dynamic_t::array_t&
dynamic_t::as_array() const {
    return get<detail::dynamic::incomplete_wrapper<array_t>>().get();
}

const dynamic_t::object_t&
dynamic_t::as_object() const {
    return get<detail::dynamic::incomplete_wrapper<object_t>>().get();
}

dynamic_t::string_t&
dynamic_t::as_string() {
    if(is_null()) {
        *this = string_t();
    }

    return get<string_t>();
}

dynamic_t::array_t&
dynamic_t::as_array() {
    if(is_null()) {
        *this = array_t();
    }

    return get<detail::dynamic::incomplete_wrapper<array_t>>().get();
}

dynamic_t::object_t&
dynamic_t::as_object() {
    if(is_null()) {
        *this = object_t();
    }

    return get<detail::dynamic::incomplete_wrapper<object_t>>().get();
}

bool
dynamic_t::is_null() const {
    return is<null_t>();
}

bool
dynamic_t::is_bool() const {
    return is<bool_t>();
}

bool
dynamic_t::is_int() const {
    return is<int_t>();
}

bool
dynamic_t::is_uint() const {
    return is<uint_t>();
}

bool
dynamic_t::is_double() const {
    return is<double_t>();
}

bool
dynamic_t::is_string() const {
    return is<string_t>();
}

bool
dynamic_t::is_array() const {
    return is<detail::dynamic::incomplete_wrapper<array_t>>();
}

bool
dynamic_t::is_object() const {
    return is<detail::dynamic::incomplete_wrapper<object_t>>();
}

struct to_string_visitor:
    public boost::static_visitor<std::string>
{
    std::string
    operator()(const dynamic_t::null_t&) const {
        return "null";
    }

    std::string
    operator()(const dynamic_t::bool_t& v) const {
        return v ? "true" : "false";
    }

    template<class T>
    std::string
    operator()(const T& v) const {
        return boost::lexical_cast<std::string>(v);
    }

    std::string
    operator()(const dynamic_t::string_t& v) const {
        return "\"" + v + "\"";
    }

    std::string
    operator()(const dynamic_t::array_t& v) const {
        std::string result = "[";

        size_t index = 0;

        if(!v.empty()) {
            result += v[index++].apply(*this);
        }

        while(index < v.size()) {
            result += ", ";
            result += v[index++].apply(*this);
        }

        return result + "]";
    }

    std::string
    operator()(const dynamic_t::object_t& v) const {
        std::string result = "{";

        dynamic_t::object_t::const_iterator it = v.begin();

        if(it != v.end()) {
            result += "\"" + it->first + "\":" + it->second.apply(*this);
            ++it;
        }

        for(; it != v.end(); ++it) {
            result += ", ";
            result += "\"" + it->first + "\":" + it->second.apply(*this);
        }

        return result + "}";
    }
};

template<>
std::string
boost::lexical_cast<std::string, cocaine::dynamic_t>(const cocaine::dynamic_t& v) {
    return v.apply(to_string_visitor());
}
