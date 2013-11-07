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

#include <string>
#include <map>
#include <type_traits>
#include <memory>

#include <boost/variant.hpp>

namespace cocaine {

class dynamic_t;

namespace detail { namespace dynamic {

    class object_t :
        public std::map<std::string, cocaine::dynamic_t>
    {
        typedef std::map<std::string, cocaine::dynamic_t>
                base_type;
    public:
        object_t() {
            // pass
        }

        template<class InputIt>
        object_t(InputIt first, InputIt last) :
            base_type(first, last)
        {
            // pass
        }

        object_t(const object_t& other) :
            base_type(other)
        {
            // pass
        }

        object_t(object_t&& other) :
            base_type(std::move(other))
        {
            // pass
        }

        object_t(std::initializer_list<value_type> init) :
            base_type(init)
        {
            // pass
        }

        object_t(const base_type& other) :
            base_type(other)
        {
            // pass
        }

        object_t(base_type&& other) :
            base_type(std::move(other))
        {
            // pass
        }

        object_t&
        operator=(const object_t& other) {
            base_type::operator=(other);
            return *this;
        }

        object_t&
        operator=(object_t&& other) {
            base_type::operator=(std::move(other));
            return *this;
        }

        using base_type::at;

        cocaine::dynamic_t&
        at(const std::string& key, cocaine::dynamic_t& def);

        const cocaine::dynamic_t&
        at(const std::string& key, const cocaine::dynamic_t& def) const;

        using base_type::operator[];

        const cocaine::dynamic_t&
        operator[](const std::string& key) const;
    };

    // Helps to use STL containers
    template<class T>
    class incomplete_wrapper {
    public:
        // These *structors are needed just to satisfy requirements of boost::variant.
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
        {
            // pass
        }

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
        {
            // pass
        }

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

    template<class T>
    struct my_decay {
        typedef typename std::remove_reference<T>::type unref;
        typedef typename std::remove_cv<unref>::type type;
    };

}} // namespace detail::dynamic

} // namespace cocaine

#endif // COCAINE_DYNAMIC_DETAIL_HPP
