#pragma once

#include <blackhole/scoped_attributes.hpp>
#include "cocaine/forwards.hpp"
#include "cocaine/trace/logger.hpp"
#include <random>
namespace cocaine { namespace tracer {

class trace_context_t {
private:
    inline
    void
    restore(std::string name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id) {
        if(trace_id) {
            span.reset(new span_t(std::move(name), trace_id, span_id, parent_id));
        }
    }

    inline
    void
    restore(span_ptr_t _span) {
        span = _span;
    }

    inline
    void
    push(std::string new_name) {
        if(!span->empty()) {
            span_ptr_t new_span(new span_t(std::move(new_name), span));
            span = new_span;
        }
    }

    inline
    void
    push_new(std::string new_name) {
        span_ptr_t new_span(new span_t(std::move(new_name), span));
        span = new_span;
    }

    inline
    void
    pop() {
        if(span->parent) {
            auto p = span->parent;
            span = p;
        }
        else {
            assert(span->empty());
        }
    }

    inline
    static void
    set_service_name(std::string name) {
        service_name() = std::move(name);
    }

    inline
    static const std::string&
    get_service_name() {
        return service_name();
    }

    inline
    trace_context_t() :
        span(new span_t())
    {}

    inline
    trace_context_t(const trace_context_t& other) :
        span(other.span)
    {}

    inline
    static trace_context_t&
    get_context() {
        if(thread_context().get() == nullptr) {
            thread_context().reset(new trace_context_t());
        }
        return *thread_context().get();
    }

    inline
    static
    boost::thread_specific_ptr<trace_context_t>&
    thread_context() {
        static boost::thread_specific_ptr<trace_context_t> ctx;
        return ctx;
    }

    inline
    static
    std::unique_ptr<tracer::logger_t>&
    logger() {
        static std::unique_ptr<tracer::logger_t> log;
        return log;
    }

    inline
    static
    void
    log(std::string message, blackhole::attribute::set_t attributes) {
        auto& log = logger();
        if(log) {
            log->log(std::move(message), std::move(attributes));
        }
    }

    inline
    static std::string& service_name() {
        static std::string service_name;
        return service_name;
    }

    span_ptr_t span;

    template <class F>
    friend class callable_wrapper_t;
    friend span_ptr_t current_span();
    friend void set_service_name(std::string name);
    friend void set_logger(std::unique_ptr<logger_t> logger);
    friend void log(std::string message);
    friend class trace_push_scope_t;
    friend class trace_restore_scope_t;
    friend class new_trace_scope_t;
    friend class trace_reset_scope_t;
    friend class span_t;
};

template <class F>
class callable_wrapper_t
{
public:
    inline
    callable_wrapper_t(std::string&& _message, F&& _f) :
        message(std::move(_message)),
        f(std::move(_f)),
        span(current_span())
    {
        if(!message.empty()) {
            trace_context_t::get_context().log("Scheduled: " + message, current_span()->attributes());
        }
    }

    template<class ...Args>
    auto
    operator()(Args&& ...args) -> decltype(std::declval<F>()(args...)) {
        auto& logger = trace_context_t::logger();
        trace_restore_scope_t scope();
        if(message.empty()) {
            scope.restore(span);
        }
        else {
            scope.restore(std::move(message), span);
        }
        if (logger) {
            auto attr_scope = logger->get_scope(span->attributes());
            return f(std::forward<Args>(args)...);
        }
        else {
            return f(std::forward<Args>(args)...);
        }
    }

private:
    std::string message;
    F f;
    span_ptr_t span;
};

template<class... Args>
auto
bind(const char* message, Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>
{
    typedef callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))> Result;
    return Result(std::move(message), std::bind(std::forward<Args>(args)...));
}

template<class... Args>
auto
bind(Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>
{
    typedef callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))> Result;
    return Result(std::string(), std::bind(std::forward<Args>(args)...));
}


template<class Method>
auto
mem_fn(const char* message, Method m) -> callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))>
{
    typedef callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))> Result;
    return Result(std::move(message), std::mem_fn(std::forward<Method>(m)));
}

inline span_ptr_t current_span() {
    return trace_context_t::get_context().span;
}

inline uint64_t generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()/2-1);
    return dis(gen);
}

inline void
set_logger(std::unique_ptr<logger_t> logger) {
    trace_context_t::logger() = std::move(logger);
}

inline void
log(std::string message) {
    auto& logger = trace_context_t::logger();
    if(logger) {
        logger->log(std::move(message), current_span()->attributes());
    }
}


blackhole::attribute::set_t
span_t::attributes() const {
    return empty() ? blackhole::attribute::set_t() :blackhole::attribute::set_t(
        {
            blackhole::attribute::make("trace_id", trace_id),
            blackhole::attribute::make("span_id", span_id),
            blackhole::attribute::make("parent_id", parent ? parent->span_id : 0),
            blackhole::attribute::make("span_name", name),
            blackhole::attribute::make("service_name", trace_context_t::get_service_name()),
        }
    );
}

trace_push_scope_t::trace_push_scope_t(std::string annotation, std::string rpc_name) {
    trace_context_t::get_context().push(std::move(rpc_name));
    auto& logger = trace_context_t::logger();
    if(logger) {
        attr_scope = logger->get_scope(current_span()->attributes());
        logger->log(std::move(annotation), current_span()->attributes());
    }
}

trace_push_scope_t::trace_push_scope_t(std::string rpc_name) {
    trace_context_t::get_context().push(std::move(rpc_name));
}

trace_push_scope_t::~trace_push_scope_t() {
    trace_context_t::get_context().pop();
}

trace_restore_scope_t::trace_restore_scope_t(std::string annotation, std::string rpc_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id) :
    old_span()
{
    restore(std::move(annotation), std::move(rpc_name), trace_id, span_id, parent_id);
}

trace_restore_scope_t::trace_restore_scope_t(std::string annotation, span_ptr_t span) :
    old_span()
{
    restore(std::move(annotation), std::move(span));
}

trace_restore_scope_t::trace_restore_scope_t(span_ptr_t span) :
    old_span()
{
    restore(std::move(span));
}

void
trace_restore_scope_t::restore(std::string annotation, span_ptr_t span) {
    assert(!old_span);
    old_span = current_span();
    trace_context_t::get_context().restore(span);
    auto& logger = trace_context_t::logger();
    if(logger) {
        attr_scope = logger->get_scope(current_span()->attributes());
        logger->log(std::move(annotation), current_span()->attributes());
    }
}

void
trace_restore_scope_t::restore(span_ptr_t span) {
    assert(!old_span);
    old_span = current_span();
    trace_context_t::get_context().restore(span);
}

void
trace_restore_scope_t::restore(std::string annotation, std::string rpc_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id) {
    assert(!old_span);
    old_span = current_span();
    trace_context_t::get_context().restore(std::move(rpc_name), trace_id, span_id, parent_id);
    auto& logger = trace_context_t::logger();
    if(logger) {
        attr_scope = logger->get_scope(current_span()->attributes());
        logger->log(std::move(annotation), current_span()->attributes());
    }
}


trace_restore_scope_t::~trace_restore_scope_t() {
    if(old_span) {
        trace_context_t::get_context().restore(old_span);
    }
}

new_trace_scope_t::new_trace_scope_t(std::string rpc_name) {
    trace_context_t::get_context().push_new(std::move(rpc_name));
    auto& logger = trace_context_t::logger();
    if(logger) {
        attr_scope = logger->get_scope(current_span()->attributes());
        logger->log(std::move("sr"), current_span()->attributes());
    }
}

new_trace_scope_t::~new_trace_scope_t() {
    auto& logger = trace_context_t::logger();
    if(logger) {
        attr_scope = logger->get_scope(current_span()->attributes());
        logger->log(std::move("ss"), current_span()->attributes());
    }
    trace_context_t::get_context().pop();
}

trace_reset_scope_t::trace_reset_scope_t() {
    assert(!old_span);
    old_span = current_span();
    trace_context_t::get_context().restore(span_ptr_t(new span_t));
}

trace_reset_scope_t::~trace_reset_scope_t() {
    trace_context_t::get_context().restore(old_span);
}

void
set_service_name(std::string name) {
    trace_context_t::set_service_name(std::move(name));
}

}}