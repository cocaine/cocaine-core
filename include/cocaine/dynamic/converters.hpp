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

#ifndef COCAINE_DYNAMIC_CONVERTERS_HPP
#define COCAINE_DYNAMIC_CONVERTERS_HPP

#include <tuple>
#include <unordered_map>

namespace cocaine {

template<>
struct dynamic_converter<dynamic_t, void> {
    typedef const dynamic_t& result_type;

    static
    const dynamic_t&
    convert(const dynamic_t& from) {
        return from;
    }

    static
    bool
    convertible(const dynamic_t&) {
        return true;
    }
};

template<>
struct dynamic_converter<bool, void> {
    typedef bool result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        return from.as_bool();
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_bool();
    }
};

template<class To>
struct dynamic_converter<To, typename std::enable_if<std::is_arithmetic<To>::value>::type> {
    typedef To result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        if(from.is_int()) {
            return from.as_int();
        } else if(from.is_uint()) {
            return from.as_uint();
        } else {
            return from.as_double();
        }
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_int() || from.is_uint() || from.is_double();
    }
};

template<class To>
struct dynamic_converter<To, typename std::enable_if<std::is_enum<To>::value>::type> {
    typedef To result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        return static_cast<result_type>(from.as_int());
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_int();
    }
};

template<>
struct dynamic_converter<std::string, void> {
    typedef const std::string& result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        return from.as_string();
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_string();
    }
};

template<>
struct dynamic_converter<const char*, void> {
    typedef const char *result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        return from.as_string().c_str();
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_string();
    }
};

template<>
struct dynamic_converter<std::vector<dynamic_t>, void> {
    typedef const std::vector<dynamic_t>& result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        return from.as_array();
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_array();
    }
};

template<class T>
struct dynamic_converter<std::vector<T>, void> {
    typedef std::vector<T> result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        std::vector<T> result;
        const dynamic_t::array_t& array = from.as_array();
        for(size_t i = 0; i < array.size(); ++i) {
            result.emplace_back(array[i].to<T>());
        }
        return result;
    }

    static
    bool
    convertible(const dynamic_t& from) {
        if(from.is_array()) {
            const dynamic_t::array_t& array = from.as_array();
            for(size_t i = 0; i < array.size(); ++i) {
                if(!array[i].convertible_to<T>()) {
                    return false;
                }
            }

            return true;
        }

        return false;
    }
};

template<class... Args>
struct dynamic_converter<std::tuple<Args...>, void> {
    typedef std::tuple<Args...> result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        if(sizeof...(Args) == from.as_array().size()) {
            if(sizeof...(Args) == 0) {
                return result_type();
            } else {
                return range_applier<sizeof...(Args) - 1>::conv(from.as_array());
            }
        } else {
            throw std::bad_cast();
        }
    }

    static
    bool
    convertible(const dynamic_t& from) {
        if(from.is_array() && sizeof...(Args) == from.as_array().size()) {
            if(sizeof...(Args) == 0) {
                return true;
            } else {
                return range_applier<sizeof...(Args) - 1>::is_conv(from.as_array());
            }
        } else {
            return false;
        }
    }

private:
    template<size_t... Idxs>
    struct range_applier;

    template<size_t First, size_t... Idxs>
    struct range_applier<First, Idxs...> :
        range_applier<First - 1, First, Idxs...>
    {
        static
        inline
        bool
        is_conv(const dynamic_t::array_t& from) {
            return from[First].convertible_to<typename std::tuple_element<First, result_type>::type>() &&
                   range_applier<First - 1, First, Idxs...>::is_conv(from);
        }
    };

    template<size_t... Idxs>
    struct range_applier<0, Idxs...> {
        static
        inline
        result_type
        conv(const dynamic_t::array_t& from) {
            return std::tuple<Args...>(
                from[0].to<typename std::tuple_element<0, result_type>::type>(),
                from[Idxs].to<typename std::tuple_element<Idxs, result_type>::type>()...
            );
        }

        static
        inline
        bool
        is_conv(const dynamic_t::array_t& from) {
            return from[0].convertible_to<typename std::tuple_element<0, result_type>::type>();
        }
    };
};

template<>
struct dynamic_converter<dynamic_t::object_t, void> {
    typedef const dynamic_t::object_t& result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        return from.as_object();
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_object();
    }
};

template<>
struct dynamic_converter<std::map<std::string, dynamic_t>, void> {
    typedef const std::map<std::string, dynamic_t>& result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        return from.as_object();
    }

    static
    bool
    convertible(const dynamic_t& from) {
        return from.is_object();
    }
};

template<class T>
struct dynamic_converter<std::map<std::string, T>, void> {
    typedef std::map<std::string, T> result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        result_type result;
        const dynamic_t::object_t& object = from.as_object();
        for(auto it = object.begin(); it != object.end(); ++it) {
            result.insert(typename result_type::value_type(it->first, it->second.to<T>()));
        }
        return result;
    }

    static
    bool
    convertible(const dynamic_t& from) {
        if(from.is_object()) {
            const dynamic_t::object_t& object = from.as_object();
            for(auto it = object.begin(); it != object.end(); ++it) {
                if(!it->second.convertible_to<T>()) {
                    return false;
                }

                return true;
            }
        }

        return false;
    }
};

template<class T>
struct dynamic_converter<std::unordered_map<std::string, T>, void> {
    typedef std::unordered_map<std::string, T> result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        result_type result;
        const dynamic_t::object_t& object = from.as_object();
        for(auto it = object.begin(); it != object.end(); ++it) {
            result.insert(typename result_type::value_type(it->first, it->second.to<T>()));
        }
        return result;
    }

    static
    bool
    convertible(const dynamic_t& from) {
        if(from.is_object()) {
            const dynamic_t::object_t& object = from.as_object();
            for(auto it = object.begin(); it != object.end(); ++it) {
                if(!it->second.convertible_to<T>()) {
                    return false;
                }

                return true;
            }
        }

        return false;
    }
};

} // namespace cocaine

#endif // COCAINE_DYNAMIC_CONVERTERS_HPP
