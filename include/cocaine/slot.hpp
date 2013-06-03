/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_DISPATCH_SLOT_HPP
#define COCAINE_DISPATCH_SLOT_HPP

#include "cocaine/api/stream.hpp"

#include "cocaine/rpc/optional.hpp"
#include "cocaine/rpc/protocol.hpp"

#include "cocaine/traits.hpp"

#include <functional>
#include <mutex>

#include <boost/function_types/function_type.hpp>

#include <boost/mpl/count_if.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/transform.hpp>

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

    template<class T>
    struct unpack {
        template<class ArgumentIterator>
        static inline
        ArgumentIterator
        apply(ArgumentIterator it, ArgumentIterator end, T& argument) {
            BOOST_VERIFY(it != end);

            try {
                // NOTE: This is the only place where the argument tuple iterator is advanced.
                io::type_traits<T>::unpack(*it++, argument);
            } catch(const msgpack::type_error& e) {
                throw cocaine::error_t("argument type mismatch");
            }

            return it;
        }
    };

    template<class T>
    struct unpack<io::optional<T>> {
        template<class ArgumentIterator>
        static inline
        ArgumentIterator
        apply(ArgumentIterator it, ArgumentIterator end, T& argument) {
            if(it != end) {
                return unpack<T>::apply(it, end, argument);
            } else {
                argument = T();
            }

            return it;
        }
    };

    template<class T, T Default>
    struct unpack<io::optional_with_default<T, Default>> {
        typedef T type;

        template<class ArgumentIterator>
        static inline
        ArgumentIterator
        apply(ArgumentIterator it, ArgumentIterator end, T& argument) {
            if(it != end) {
                return unpack<T>::apply(it, end, argument);
            } else {
                argument = Default;
            }

            return it;
        }
    };

    template<class It, class End>
    struct invoke_impl {
        template<class F, class ArgumentIterator, typename... Args>
        static inline
        typename result_of<F>::type
        apply(const F& callable, ArgumentIterator it, ArgumentIterator end, Args&&... args) {
            typedef typename mpl::deref<It>::type argument_type;
            typename io::detail::unwrap_type<argument_type>::type argument;

            it = unpack<argument_type>::apply(it, end, argument);

            return invoke_impl<typename mpl::next<It>::type, End>::apply(
                callable,
                it,
                end,
                std::forward<Args>(args)...,
                std::move(argument)
            );
        }
    };

    template<class End>
    struct invoke_impl<End, End> {
        template<class F, class ArgumentIterator, typename... Args>
        static inline
        typename result_of<F>::type
        apply(const F& callable, ArgumentIterator, ArgumentIterator, Args&&... args) {
            return callable(std::forward<Args>(args)...);
        }
    };
}

// Slot invocation mechanics

#if defined(__GNUC__) && !defined(HAVE_GCC46)
    #pragma GCC diagnostic ignored "-Wtype-limits"
#endif

template<class Sequence>
struct invoke {
    template<class F>
    static inline
    typename detail::result_of<F>::type
    apply(const F& callable, const msgpack::object& unpacked) {
        const size_t required = mpl::count_if<
            Sequence,
            mpl::lambda<io::detail::is_required<mpl::arg<1>>>
        >::value;

        if(unpacked.type != msgpack::type::ARRAY) {
            throw cocaine::error_t("argument sequence type mismatch");
        }

        #if defined(__clang__)
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wtautological-compare"
        #elif defined(__GNUC__) && defined(HAVE_GCC46)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wtype-limits"
        #endif

        // NOTE: In cases when the callable is nullary or every parameter is optional, this
        // comparison becomes tautological and emits dead code.
        if(unpacked.via.array.size < required) {
            throw cocaine::error_t(
                "argument sequence length mismatch - expected at least %d, got %d",
                required,
                unpacked.via.array.size
            );
        }

        #if defined(__clang__)
            #pragma clang diagnostic pop
        #elif defined(__GNUC__) && defined(HAVE_GCC46)
            #pragma GCC diagnostic pop
        #endif

        typedef typename mpl::begin<Sequence>::type begin;
        typedef typename mpl::end<Sequence>::type end;

        const std::vector<msgpack::object> args(
            unpacked.via.array.ptr,
            unpacked.via.array.ptr + unpacked.via.array.size
        );

        return detail::invoke_impl<begin, end>::apply(callable, args.begin(), args.end());
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
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) = 0;

public:
    std::string
    name() const {
        return m_name;
    }

private:
    const std::string m_name;
};

template<class R, class Sequence>
struct basic_slot:
    public slot_concept_t
{
    typedef typename mpl::transform<
        Sequence,
        mpl::lambda<io::detail::unwrap_type<mpl::arg<1>>>
    >::type sequence_type;

    typedef typename ft::function_type<
        typename mpl::push_front<sequence_type, R>::type
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
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        io::type_traits<R>::pack(m_packer, invoke<Sequence>::apply(
            this->m_callable,
            unpacked
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
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        invoke<Sequence>::apply(this->m_callable, unpacked);

        // This is needed so that service clients could detect operation completion.
        upstream->close();
    }
};

// Deferred slot

namespace detail {
    struct state_t {
        state_t();

        template<class T>
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
        abort(int code, const std::string& reason);

        void
        close();

        void
        attach(const api::stream_ptr_t& upstream);

    private:
        msgpack::sbuffer m_buffer;
        msgpack::packer<msgpack::sbuffer> m_packer;

        int m_code;
        std::string m_reason;

        bool m_completed,
             m_failed;

        api::stream_ptr_t m_upstream;
        std::mutex m_mutex;
    };
}

template<class T>
struct deferred {
    deferred():
        m_state(new detail::state_t())
    { }

    void
    attach(const api::stream_ptr_t& upstream) {
        m_state->attach(upstream);
    }

    void
    write(const T& value) {
        m_state->write(value);
    }

    void
    abort(int code, const std::string& reason) {
        m_state->abort(code, reason);
    }

private:
    const std::shared_ptr<detail::state_t> m_state;
};

template<>
struct deferred<void> {
    deferred():
        m_state(new detail::state_t())
    { }

    void
    attach(const api::stream_ptr_t& upstream) {
        m_state->attach(upstream);
    }

    void
    close() {
        m_state->close();
    }

    void
    abort(int code, const std::string& reason) {
        m_state->abort(code, reason);
    }

private:
    const std::shared_ptr<detail::state_t> m_state;
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
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        auto deferred = invoke<Sequence>::apply(
            this->m_callable,
            unpacked
        );

        deferred.attach(upstream);
    }
};

} // namespace cocaine

#endif
