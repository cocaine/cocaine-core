/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#ifndef TRACE_HPP
#define TRACE_HPP

#include "cocaine/traits/literal.hpp"

#include <boost/thread/tss.hpp>
#include <boost/optional.hpp>

#include <random>

namespace cocaine {

// No scope here
class empty_atttribute_scope_t {
public:
    //Don't do anything here by default
    template<class ...Args>
    empty_atttribute_scope_t(const Args& ...) {}
};

template<class ScopedAttribute>
struct scoped_attribute_wrapper {
    template<class Logger, class Trace>
    scoped_attribute_wrapper(Logger& log, const Trace& t) :
        attr(log, t.attributes())
    {}

    template<class Logger, class Trace>
    scoped_attribute_wrapper(Logger& log, const boost::optional<Trace>& t) :
        attr(log, t ? t->attributes() : typename Trace::attribute_set_t())
    {}
    ScopedAttribute attr;
};

template<>
struct scoped_attribute_wrapper<empty_atttribute_scope_t> {
    template<class Logger, class Trace>
    scoped_attribute_wrapper(const Logger&, const Trace&) {}
};

template<class Logger, class ScopedAttribute, class AttributeSet>
class trace {
public:
    typedef AttributeSet attribute_set_t;
    template<size_t N>
    struct stack_str_t {
        static constexpr size_t max_size = N;
        char blob[N+1];
        size_t size;

        stack_str_t(const char* lit) {
            memcpy(blob, lit, std::min(N, strlen(lit)));
            blob[N] = '\0';
        }

        stack_str_t(const char* lit, size_t sz) {
            memcpy(blob, lit, std::min(N, sz));
            blob[N] = '\0';
        }

        stack_str_t(const std::string& source) {
            memcpy(blob, source.c_str(), std::min(N, source.size()));
            blob[N] = '\0';
        }

        stack_str_t() :
            blob(),
            size(0)
        {}
    };

    template <class F>
    class callable_wrapper_t;

    class restore_scope_t {
    public:
        restore_scope_t(boost::optional<trace> new_trace) :
            old_span(),
            attribute(*trace::get_logger(), new_trace)
        {
            if(new_trace) {
                old_span = trace::current();
                trace::current() = new_trace.get();
            }
        }
        ~restore_scope_t() {
            if(old_span) {
                trace::current() = old_span.get();
            }
        }
    private:
        boost::optional<trace> old_span;
        scoped_attribute_wrapper<ScopedAttribute> attribute;
    };

    class push_scope_t {
    public:
        template<class ServiceStr, class RpcStr>
        push_scope_t(const RpcStr& _rpc_name, const ServiceStr& _service_name):
            attribute(*trace::get_logger(), push())
        {
        }

        ~push_scope_t() {
            trace::current().pop();
        }

    private:
        trace&
        push() {
            return trace::current().push();
        }

        scoped_attribute_wrapper<ScopedAttribute> attribute;
    };

    trace() :
        trace_id(),
        span_id(),
        parent_id(),
        parent_parent_id(),
        start_time_us(),
        last_time_us(),
        rpc_name(),
        service_name()
    {}

    /*
    trace(const trace& other) :
        trace_id(other.trace_id),
        span_id(other.span_id),
        parent_id(other.parent_id),
        parent_parent_id(other.parent_id),
        start_time_us(other.start_time_us),
        last_time_us(other.last_time_us),
        name(other.name)
    {}
    */

    template<class ServiceStr, class RpcStr>
    trace(uint64_t _trace_id,
          uint64_t _span_id,
          uint64_t _parent_id,
          const RpcStr& _rpc_name,
          const ServiceStr& _service_name) :
        trace_id(_trace_id),
        span_id(_span_id),
        parent_id(_parent_id),
        parent_parent_id(),
        start_time_us(),
        last_time_us(),
        rpc_name(_rpc_name),
        service_name(_service_name)
    {
    }

    static
    trace&
    current() {
        static boost::thread_specific_ptr<trace> t;
        if(t.get() == nullptr) {
            t.reset(new trace());
        }
        return *t.get();
    }

    static
    Logger*
    get_logger() {
        return logger;
    }

    static
    void
    set_logger(Logger* _logger) {
        logger = _logger;
    }

//    static
//    void
//    set_default_service_name(const literal_t& new_name) {
//        default_service_name = new_name;
//    }

    template<class... Args>
    static
    auto
    bind(Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>;

    template<class Method>
    static
    auto
    mem_fn(Method m) -> callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))>;

    uint64_t
    get_parent_id() const {
        return parent_id;
    }

    uint64_t
    get_trace_id() const {
        return trace_id;
    }

    uint64_t
    get_id() const {
        return span_id;
    }

    trace& pop() {
        assert(parent_parent_id != 0);
        span_id = parent_id;
        parent_id = parent_parent_id;
        parent_parent_id = 0;
    }

    trace& push() {
        parent_parent_id = parent_id;
        parent_id = span_id;
        span_id = generate_id();
    }

    uint64_t generate_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()/2-1);
        return dis(gen);
    }

    AttributeSet
    attributes() const {
        return {
            {"trace_id", {trace_id}},
            {"span_id", {trace_id}},
            {"parent_id", {trace_id}},
            {"rpc_name", {rpc_name.blob}},
            {"service_name", {service_name.blob}}
        };
    }
private:
    uint64_t trace_id;
    uint64_t span_id;
    uint64_t parent_id;
    uint64_t parent_parent_id;
    uint64_t start_time_us;
    uint64_t last_time_us;
    stack_str_t<16> rpc_name;
    stack_str_t<16> service_name;

    static Logger* logger;
};

template<class Logger, class ScopedAttribute, class AttributeSet>
Logger* trace<Logger, ScopedAttribute, AttributeSet>::logger = nullptr;

}
#include "cocaine/trace/trace_impl.hpp"

#endif // TRACE_HPP

