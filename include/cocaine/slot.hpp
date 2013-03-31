/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_DISPATCH_SLOT_HPP
#define COCAINE_DISPATCH_SLOT_HPP

#include "cocaine/common.hpp"
#include "cocaine/traits.hpp"

#include "cocaine/api/stream.hpp"

#include <functional>
#include <mutex>

#include <boost/function_types/function_type.hpp>
#include <boost/mpl/push_front.hpp>

namespace cocaine {

namespace ft = boost::function_types;
namespace mpl = boost::mpl;

namespace detail {
    template<class T>
    struct depend {
        typedef void type;
    };

    template<class F, class = void>
    struct result_of {
        typedef typename std::result_of<F>::type type;
    };

    template<class F>
    struct result_of<
        F,
        typename depend<typename F::result_type>::type
    >
    {
        typedef typename F::result_type type;
    };

    template<class It, class End>
    struct invoke_impl {
        template<class F, typename... Args>
        static inline
        typename result_of<F>::type
        apply(const F& callable, const msgpack::object * ptr, Args&&... args) {
            typedef typename mpl::deref<It>::type argument_type;
            typedef typename mpl::next<It>::type next;

            argument_type argument;

            try {
                io::type_traits<argument_type>::unpack(*ptr, argument);
            } catch(const msgpack::type_error& e) {
                throw cocaine::error_t("argument type mismatch");
            }

            return invoke_impl<next, End>::apply(
                callable,
                ++ptr,
                std::forward<Args>(args)...,
                std::move(argument)
            );
        }
    };

    template<class End>
    struct invoke_impl<End, End> {
        template<class F, typename... Args>
        static inline
        typename result_of<F>::type
        apply(const F& callable, const msgpack::object * /* ptr */, Args&&... args) {
            return callable(std::forward<Args>(args)...);
        }
    };
}

// Slot invocation mechanics

template<class Sequence>
struct invoke {
    template<class F>
    static inline
    typename detail::result_of<F>::type
    apply(const F& callable, const msgpack::object& args) {
        if(args.type != msgpack::type::ARRAY || args.via.array.size != mpl::size<Sequence>::value) {
            throw cocaine::error_t("argument sequence mismatch");
        }

        typedef typename mpl::begin<Sequence>::type begin;
        typedef typename mpl::end<Sequence>::type end;

        return detail::invoke_impl<begin, end>::apply(callable, args.via.array.ptr);
    }
};

// Slot basics

struct slot_concept_t {
    slot_concept_t(const std::string& name):
        m_name(name)
    { }

    virtual
   ~slot_concept_t() {
       // Empty.
    }

    virtual
    void
    operator()(const api::stream_ptr_t& upstream, const msgpack::object& args) = 0;

public:
    virtual
    std::string
    describe() const {
        return m_name;
    }

private:
    const std::string m_name;
};

template<class R, class Sequence>
struct basic_slot:
    public slot_concept_t
{
    typedef typename ft::function_type<
        typename mpl::push_front<Sequence, R>::type
    >::type function_type;

    typedef std::function<function_type> callable_type;

    basic_slot(const std::string& name, callable_type callable):
        slot_concept_t(name),
        m_callable(callable)
    { }

protected:
    const callable_type m_callable;
};

// Blocking slot

template<class R, class Sequence>
struct blocking_slot:
    public basic_slot<R, Sequence>
{
    typedef basic_slot<R, Sequence> base_type;
    typedef typename base_type::callable_type callable_type;

    blocking_slot(const std::string& name, callable_type callable):
        base_type(name, callable),
        m_packer(m_buffer)
    { }

    virtual
    void
    operator()(const api::stream_ptr_t& upstream, const msgpack::object& args) {
        io::type_traits<R>::pack(m_packer, invoke<Sequence>::apply(
            this->m_callable,
            args
        ));

        upstream->write(m_buffer.data(), m_buffer.size());
        upstream->close();

        m_buffer.clear();
    }

private:
    msgpack::sbuffer m_buffer;
    msgpack::packer<msgpack::sbuffer> m_packer;
};

// Blocking slot specialization for void functions

template<class Sequence>
struct blocking_slot<void, Sequence>:
    public basic_slot<void, Sequence>
{
    typedef basic_slot<void, Sequence> base_type;
    typedef typename base_type::callable_type callable_type;

    blocking_slot(const std::string& name, callable_type callable):
        base_type(name, callable)
    { }

    virtual
    void
    operator()(const api::stream_ptr_t& upstream, const msgpack::object& args) {
        invoke<Sequence>::apply(
            this->m_callable,
            args
        );

        upstream->close();
    }
};

// Deferred slot

template<class T>
struct deferred {
    deferred():
        m_state(new state_t())
    { }

    void
    write(const T& value) {
        m_state->write(value);
    }

    void
    abort(error_code code, const std::string& reason) {
        m_state->abort(code, reason);
    }

    void
    attach(const api::stream_ptr_t& upstream) {
        m_state->attach(upstream);
    }

private:
    struct state_t {
        state_t():
            m_packer(m_buffer),
            m_completed(false),
            m_failed(false)
        { }

        void
        write(const T& value) {
            std::unique_lock<std::mutex> lock(m_mutex);

            if(m_completed) {
                return;
            }

            io::type_traits<T>::pack(m_packer, value);

            if(m_upstream) {
                m_upstream->write(m_buffer.data(), m_buffer.size());
                m_upstream->close();
            }

            m_completed = true;
        }

        void
        abort(const std::exception_ptr& e) {
            std::unique_lock<std::mutex> lock(m_mutex);

            if(m_completed) {
                return;
            }

            if(m_upstream) {
                try {
                    std::rethrow_exception(e);
                } catch(const std::exception& e) {
                    m_upstream->error(invocation_error, e.what());
                    m_upstream->close();
                }
            } else {
                m_e = e;
            }

            m_failed = true;
        }

        void
        attach(const api::stream_ptr_t& upstream) {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_upstream = upstream;

            if(m_completed) {
                m_upstream->write(m_buffer.data(), m_buffer.size());
                m_upstream->close();
            } else if(m_failed) {
                try {
                    std::rethrow_exception(m_e);
                } catch(const std::exception& e) {
                    m_upstream->error(invocation_error, e.what());
                    m_upstream->close();
                }
            }
        }

    private:
        msgpack::sbuffer m_buffer;
        msgpack::packer<msgpack::sbuffer> m_packer;
        std::exception_ptr m_e;

        bool m_completed,
             m_failed;

        api::stream_ptr_t m_upstream;
        std::mutex m_mutex;
    };

    const std::shared_ptr<state_t> m_state;
};

template<class R, class Sequence>
struct deferred_slot:
    public basic_slot<R, Sequence>
{
    typedef basic_slot<R, Sequence> base_type;
    typedef typename base_type::callable_type callable_type;

    deferred_slot(const std::string& name, callable_type callable):
        base_type(name, callable)
    { }

    virtual
    void
    operator()(const api::stream_ptr_t& upstream, const msgpack::object& args) {
        auto deferred = invoke<Sequence>::apply(
            this->m_callable,
            args
        );

        deferred.attach(upstream);
    }
};

} // namespace cocaine

#endif
