#pragma once

#include <blackhole/scoped_attributes.hpp>
#include "cocaine/forwards.hpp"

#include "cocaine/trace/logger.hpp"

namespace cocaine { namespace tracer {

/*
 ********************************************
 ************** Public API ******************
 ********************************************
*/

struct span_t;
typedef std::shared_ptr<span_t> span_ptr_t;

template <class F>
class callable_wrapper_t;

inline
span_ptr_t
current_span();

inline
void
set_service_name();

class trace_push_scope_t {
public:
    inline
    trace_push_scope_t(std::string rpc_name);

    inline
    ~trace_push_scope_t();
};

class trace_restore_scope_t {
public:
    inline
    trace_restore_scope_t(std::string rpc_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id);

    inline
    trace_restore_scope_t(span_ptr_t span);

    inline
    ~trace_restore_scope_t();
};


template<class... Args>
auto
bind(std::string message, Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>;


struct span_t :
    public std::enable_shared_from_this<span_t>
{
private:
    uint64_t trace_id;
    uint64_t span_id;
    uint64_t start_time_us;
    uint64_t last_time_us;
    std::shared_ptr<span_t> parent;
    std::string name;

    span_t() :
        trace_id(),
        span_id(),
        start_time_us(),
        last_time_us(),
        parent(nullptr),
        name()
    {}

    span_t(std::string _name, uint64_t _trace_id, uint64_t _span_id, uint64_t _parent_id) :
        trace_id(_trace_id ? _trace_id : generate_id()),
        span_id(_span_id ? _span_id : trace_id),
        start_time_us(cur_time()),
        last_time_us(start_time_us),
        parent(new span_t("", trace_id, _parent_id)),
        name(_name)
    {}


    span_t(std::string _name, std::shared_ptr<span_t> _parent) :
        trace_id(_parent->empty() ? generate_id() : _parent->trace_id),
        span_id(_parent->empty() ? trace_id : generate_id()),
        start_time_us(cur_time()),
        last_time_us(start_time_us),
        parent(std::move(_parent)),
        name(_name)
    {}

    static
    inline
    uint64_t
    cur_time(){
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    inline bool
    empty() const {
        return span_id == 0;
    }

    inline
    uint64_t
    get_parent_id() const {
        return parent && !parent->empty() ? parent->span_id : 0;
    }

    inline
    uint64_t
    get_trace_id() const {
        return trace_id;
    }

    inline
    uint64_t
    get_id() const {
        return span_id;
    }

    inline
    const std::string&
    get_name() const {
        return name;
    }

    inline
    void
    set_name(std::string new_name) {
        last_time_us = cur_time();
        name = std::move(new_name);
    }

    inline
    uint64_t
    get_last_time() const {
        return last_time_us;
    }

    inline
    uint64_t
    duration() const {
        return cur_time() - last_time_us;
    }

    inline
    uint64_t
    full_duration() const {
        return cur_time() - start_time_us;
    }

    inline
    blackhole::attribute::set_t
    attributes() const;

    span_t(std::string _name, uint64_t _trace_id, uint64_t _span_id) :
        trace_id(_trace_id),
        span_id(_span_id),
        start_time_us(cur_time()),
        last_time_us(start_time_us),
        parent(nullptr),
        name(_name)
    {}
    friend class trace_context_t;
};

class trace_context_t {
private:
    inline
    void
    restore (std::string name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id) {
        span.reset(new span_t(std::move(name), trace_id, span_id, parent_id));
    }

    inline
    void
    restore (span_ptr_t _span) {
        span = _span;
    }

    inline
    void
    push (std::string new_name) {
        span_ptr_t new_span = std::make_shared<span_t>(std::move(new_name), span);
        span = new_span;
    }

    inline
    void
    pop () {
        if(!span->empty()) {
            assert(span->parent);
            auto p = span->parent;
            span = p;
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
    std::shared_ptr<tracer::logger_t>&
    default_logger() {
        static std::shared_ptr<tracer::logger_t> log;
        return log;
    }

    inline
    static
    void
    log(std::string message, blackhole::attribute_set_t attributes) {
        auto log = default_logger();
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
    friend void restore(std::string event_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id);
    friend void restore(span_ptr_t span);
    friend void reset();
    friend span_ptr_t current_span();
    friend void push(std::string app_name);
    friend void pop();
    friend class trace_push_scope_t;
};

template <class F>
class callable_wrapper_t
{
public:
    inline
    callable_wrapper_t(std::string&& _message, F&& _f) :
        message(std::move(_message)),
        f(std::move(_f)),
        trace_context(trace_context_t::get_context())
    {
          COCAINE_LOG_INFO(trace_context.default_logger(), "%s", message.c_str());
#endif
    }
    struct cleanup_t {

        cleanup_t(trace_context_t* _old_context) :
            old_context(_old_context)
        {}

        ~cleanup_t() {
            trace_context_t::thread_context().reset(old_context);
        }

        trace_context_t* old_context;
    };

    template<class ...Args>
    auto
    operator()(Args&& ...args) -> decltype(std::declval<F>()(args...)) {
        cleanup_t c(trace_context_t::thread_context().release());
        trace_context_t::thread_context().reset(new trace_context_t(trace_context));
        #ifdef COCAINE_TRACE_USE_LOG
        auto& span = trace_context_t::get_context().span;
        blackhole::scoped_attributes_t attr(*trace_context_t::default_logger(), span->attributes());
        COCAINE_LOG_INFO(trace_context.default_logger(), "Invoke: %s", message.c_str());
        #endif
        return f(std::forward<Args>(args)...);

    }

private:
    std::string message;
    F f;
    trace_context_t trace_context;


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

void restore(std::string event_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id) {
    trace_context_t::get_context().restore(std::move(event_name), trace_id, span_id, parent_id);
}

void restore(span_ptr_t span) {
    trace_context_t::get_context().restore(std::move(span));
}

inline void reset() {
    trace_context_t::get_context().span.reset(new span_t());
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

inline void push(std::string app_name) {
    trace_context_t::get_context().push(std::move(app_name));
}

inline void pop() {
    trace_context_t::get_context().pop();
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

trace_push_scope_t::trace_push_scope_t(std::string rpc_name) {
    trace_context_t::get_context().push(std::move(rpc_name));
}
trace_push_scope_t::~trace_push_scope_t() {
    trace_context_t::get_context().pop();
}

}}