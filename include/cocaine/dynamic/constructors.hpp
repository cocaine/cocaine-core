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

#ifndef COCAINE_DYNAMIC_CONSTRUCTORS_HPP
#define COCAINE_DYNAMIC_CONSTRUCTORS_HPP

#include <tuple>
#include <unordered_map>

namespace cocaine {

template<>
struct dynamic_constructor<dynamic_t::null_t> {
    static const bool enable = true;

    static inline
    void
    convert(const dynamic_t::null_t& from, dynamic_t::value_t& to) {
        to = from;
    }
};

template<>
struct dynamic_constructor<bool> {
    static const bool enable = true;

    static inline
    void
    convert(bool from, dynamic_t::value_t& to) {
        to = dynamic_t::bool_t(from);
    }
};

template<class From>
struct dynamic_constructor<
    From,
    typename std::enable_if<std::is_integral<From>::value && std::is_unsigned<From>::value>::type
>
{
    static const bool enable = true;

    static inline
    void
    convert(From from, dynamic_t::value_t& to) {
        to = dynamic_t::uint_t(from);
    }
};

template<class From>
struct dynamic_constructor<
    From,
    typename std::enable_if<std::is_integral<From>::value && std::is_signed<From>::value>::type
>
{
    static const bool enable = true;

    static inline
    void
    convert(From from, dynamic_t::value_t& to) {
        to = dynamic_t::int_t(from);
    }
};

template<class From>
struct dynamic_constructor<
    From,
    typename std::enable_if<std::is_enum<From>::value>::type
>
{
    static const bool enable = true;

    static inline
    void
    convert(const From& from, dynamic_t::value_t& to) {
        if (std::is_signed<typename std::underlying_type<From>::type>::value) {
            to = dynamic_t::int_t(from);
        } else {
            to = dynamic_t::uint_t(from);
        }
    }
};

template<class From>
struct dynamic_constructor<
    From,
    typename std::enable_if<std::is_floating_point<From>::value>::type
>
{
    static const bool enable = true;

    static inline
    void
    convert(From from, dynamic_t::value_t& to) {
        to = dynamic_t::double_t(from);
    }
};

template<size_t N>
struct dynamic_constructor<char[N]> {
    static const bool enable = true;

    static inline
    void
    convert(const char* from, dynamic_t::value_t& to) {
        to = dynamic_t::string_t();
        boost::get<dynamic_t::string_t>(to).assign(from, N - 1);
    }
};

template<>
struct dynamic_constructor<std::string> {
    static const bool enable = true;

    static inline
    void
    convert(const std::string& from, dynamic_t::value_t& to) {
        to = from;
    }

    static inline
    void
    convert(std::string&& from, dynamic_t::value_t& to) {
        to = dynamic_t::string_t();
        boost::get<dynamic_t::string_t>(to) = std::move(from);
    }
};

template<>
struct dynamic_constructor<dynamic_t::array_t> {
    static const bool enable = true;

    template<class Array>
    static inline
    void
    convert(Array&& from, dynamic_t::value_t& to) {
        to = detail::dynamic::incomplete_wrapper<dynamic_t::array_t>();
        boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).set(std::forward<Array>(from));
    }
};

template<class T>
struct dynamic_constructor<
    std::vector<T>,
    typename std::enable_if<!std::is_same<T, dynamic_t>::value>::type
>
{
    static const bool enable = true;

    static inline
    void
    convert(const std::vector<T>& from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::array_t>::convert(dynamic_t::array_t(), to);

        auto& array = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).get();
        array.reserve(from.size());

        for(size_t i = 0; i < from.size(); ++i) {
            array.emplace_back(from[i]);
        }
    }

    static inline
    void
    convert(std::vector<T>&& from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::array_t>::convert(dynamic_t::array_t(), to);

        auto& array = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).get();
        array.reserve(from.size());

        for(size_t i = 0; i < from.size(); ++i) {
            array.emplace_back(std::move(from[i]));
        }
    }
};

template<class T, size_t N>
struct dynamic_constructor<T[N]> {
    static const bool enable = true;

    static inline
    void
    convert(const T (&from)[N], dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::array_t>::convert(dynamic_t::array_t(), to);

        auto& array = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).get();
        array.reserve(N);

        for(size_t i = 0; i < N; ++i) {
            array.emplace_back(from[i]);
        }
    }

    static inline
    void
    convert(T (&&from)[N], dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::array_t>::convert(dynamic_t::array_t(), to);

        auto& array = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).get();
        array.reserve(N);

        for(size_t i = 0; i < N; ++i) {
            array.emplace_back(std::move(from[i]));
        }
    }
};

template<class... Args>
struct dynamic_constructor<std::tuple<Args...>> {
    static const bool enable = true;

    template<size_t N, size_t I, class... Args2>
    struct copy_tuple_to_vector {
        static inline
        void
        convert(const std::tuple<Args2...>& from, dynamic_t::array_t& to) {
            to.emplace_back(std::get<I - 1>(from));
            copy_tuple_to_vector<N, I + 1, Args2...>::convert(from, to);
        }
    };

    template<size_t N, class... Args2>
    struct copy_tuple_to_vector<N, N, Args2...> {
        static inline
        void
        convert(const std::tuple<Args2...>& from, dynamic_t::array_t& to) {
            to.emplace_back(std::get<N - 1>(from));
        }
    };

    template<class... Args2>
    struct copy_tuple_to_vector<0, 1, Args2...> {
        static inline
        void
        convert(const std::tuple<Args2...>&, dynamic_t::array_t&) {
            // Empty.
        }
    };

    template<size_t N, size_t I, class... Args2>
    struct move_tuple_to_vector {
        static inline
        void
        convert(std::tuple<Args2...>& from, dynamic_t::array_t& to) {
            to.emplace_back(std::move(std::get<I - 1>(from)));
            move_tuple_to_vector<N, I + 1, Args2...>::convert(from, to);
        }
    };

    template<size_t N, class... Args2>
    struct move_tuple_to_vector<N, N, Args2...> {
        static inline
        void
        convert(std::tuple<Args2...>& from, dynamic_t::array_t& to) {
            to.emplace_back(std::move(std::get<N - 1>(from)));
        }
    };

    template<class... Args2>
    struct move_tuple_to_vector<0, 1, Args2...> {
        static inline
        void
        convert(std::tuple<Args2...>&, dynamic_t::array_t&) {
            // Empty.
        }
    };

    static inline
    void
    convert(const std::tuple<Args...>& from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::array_t>::convert(dynamic_t::array_t(), to);

        auto& array = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).get();
        array.reserve(sizeof...(Args));

        copy_tuple_to_vector<sizeof...(Args), 1, Args...>::convert(from, array);
    }

    static inline
    void
    convert(std::tuple<Args...>&& from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::array_t>::convert(dynamic_t::array_t(), to);

        auto& array = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::array_t>>(to).get();
        array.reserve(sizeof...(Args));

        move_tuple_to_vector<sizeof...(Args), 1, Args...>::convert(from, array);
    }
};

template<>
struct dynamic_constructor<dynamic_t::object_t> {
    static const bool enable = true;

    template<class Object>
    static inline
    void
    convert(Object&& from, dynamic_t::value_t& to) {
        to = detail::dynamic::incomplete_wrapper<dynamic_t::object_t>();
        boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::object_t>>(to).set(std::forward<Object>(from));
    }
};

template<>
struct dynamic_constructor<std::map<std::string, dynamic_t>> {
    static const bool enable = true;

    template<class Object>
    static inline
    void
    convert(Object&& from, dynamic_t::value_t& to) {
        to = detail::dynamic::incomplete_wrapper<dynamic_t::object_t>();
        boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::object_t>>(to).set(std::forward<Object>(from));
    }
};

template<class T>
struct dynamic_constructor<
    std::map<std::string, T>,
    typename std::enable_if<!std::is_same<T, dynamic_t>::value>::type
>
{
    static const bool enable = true;

    static inline
    void
    convert(const std::map<std::string, T>& from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::object_t>::convert(dynamic_t::object_t(), to);

        auto& object = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::object_t>>(to).get();

        for(auto it = from.begin(); it != from.end(); ++it) {
            object.insert(dynamic_t::object_t::value_type(it->first, it->second));
        }
    }

    static inline
    void
    convert(std::map<std::string, T>&& from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::object_t>::convert(dynamic_t::object_t(), to);

        auto& object = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::object_t>>(to).get();

        for(auto it = from.begin(); it != from.end(); ++it) {
            object.insert(dynamic_t::object_t::value_type(it->first, std::move(it->second)));
        }
    }
};

template<class T>
struct dynamic_constructor<std::unordered_map<std::string, T>> {
    static const bool enable = true;

    static inline
    void
    convert(const std::unordered_map<std::string, T>& from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::object_t>::convert(dynamic_t::object_t(), to);

        auto& object = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::object_t>>(to).get();

        for(auto it = from.begin(); it != from.end(); ++it) {
            object.insert(it->first, it->second);
        }
    }

    static inline
    void
    convert(std::unordered_map<std::string, T>&& from, dynamic_t::value_t& to) {
        dynamic_constructor<dynamic_t::object_t>::convert(dynamic_t::object_t(), to);

        auto& object = boost::get<detail::dynamic::incomplete_wrapper<dynamic_t::object_t>>(to).get();

        for(auto it = from.begin(); it != from.end(); ++it) {
            object.insert(it->first, std::move(it->second));
        }
    }
};

} // namespace cocaine

#endif // COCAINE_DYNAMIC_CONSTRUCTORS_HPP
