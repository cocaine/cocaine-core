#include <cocaine/dynamic/dynamic.hpp>

using namespace cocaine;

cocaine::dynamic_t&
detail::dynamic::object_t::at(const std::string& key, cocaine::dynamic_t& def) {
    auto it = find(key);
    if(it == end()) {
        return def;
    } else {
        return it->second;
    }
}

const cocaine::dynamic_t&
detail::dynamic::object_t::at(const std::string& key, const cocaine::dynamic_t& def) const {
    auto it = find(key);
    if(it == end()) {
        return def;
    } else {
        return it->second;
    }
}

const cocaine::dynamic_t&
detail::dynamic::object_t::operator[](const std::string& key) const {
    return at(key);
}

struct is_empty_visitor :
    public boost::static_visitor<bool>
{
    bool
    operator()(const dynamic_t::null_t&) const {
        return true;
    }

    bool
    operator()(const dynamic_t::bool_t&) const {
        return false;
    }

    bool
    operator()(const dynamic_t::int_t&) const {
        return false;
    }

    bool
    operator()(const dynamic_t::double_t&) const {
        return false;
    }

    bool
    operator()(const dynamic_t::string_t& v) const {
        return v.empty();
    }

    bool
    operator()(const dynamic_t::array_t& v) const {
        return v.empty();
    }

    bool
    operator()(const dynamic_t::object_t& v) const {
        return v.empty();
    }
};

struct move_visitor :
    public boost::static_visitor<>
{
    move_visitor(dynamic_t& dest) :
        m_dest(dest)
    {
        // pass
    }

    template<class T>
    void
    operator()(T& v) const {
        m_dest = std::move(v);
    }

private:
    dynamic_t& m_dest;
};

dynamic_t::dynamic_t() :
    m_value(null_t())
{
// pass
}

dynamic_t::dynamic_t(const dynamic_t& other) :
    m_value(other.m_value)
{
    // pass
}

dynamic_t::dynamic_t(dynamic_t&& other) :
    m_value(null_t())
{
    other.apply(move_visitor(*this));
}

dynamic_t&
dynamic_t::operator=(const dynamic_t& other) {
    m_value = other.m_value;
    return *this;
}

dynamic_t&
dynamic_t::operator=(dynamic_t&& other) {
    other.apply(move_visitor(*this));
    return *this;
}

bool
dynamic_t::operator==(const dynamic_t& other) const {
    return m_value == other.m_value;
}

bool
dynamic_t::operator!=(const dynamic_t& other) const {
    return !(m_value == other.m_value);
}

dynamic_t::bool_t
dynamic_t::as_bool() const {
    return get<bool_t>();
}

dynamic_t::int_t
dynamic_t::as_int() const {
    return get<int_t>();
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
    return get<array_t>();
}

const dynamic_t::object_t&
dynamic_t::as_object() const {
    return get<object_t>();
}

dynamic_t::string_t&
dynamic_t::as_string() {
    if(is_null()) {
        m_value = string_t();
    }

    return get<string_t>();
}

dynamic_t::array_t&
dynamic_t::as_array() {
    if(is_null()) {
        m_value = array_t();
    }

    return get<array_t>();
}

dynamic_t::object_t&
dynamic_t::as_object() {
    if(is_null()) {
        m_value = object_t();
    }

    return get<object_t>();
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
dynamic_t::is_double() const {
    return is<double_t>();
}

bool
dynamic_t::is_string() const {
    return is<string_t>();
}

bool
dynamic_t::is_array() const {
    return is<array_t>();
}

bool
dynamic_t::is_object() const {
    return is<object_t>();
}
