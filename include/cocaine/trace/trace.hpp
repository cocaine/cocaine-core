#pragma once

#include <blackhole/scoped_attributes.hpp>
#include "cocaine/forwards.hpp"

#include <random>

namespace cocaine { namespace tracer {

class attribute_scope_t;
class span_t;
typedef std::shared_ptr<span_t> span_ptr_t;

template <class F>
class callable_wrapper_t;

class logger_t;
/*
 ********************************************
 ************** Public API ******************
 ********************************************
*/

inline
span_ptr_t
current_span();

inline
void
set_service_name(std::string name);

inline void
set_logger(std::unique_ptr<logger_t> logger);

inline void
log(std::string message);

inline void
disable_span_log();

class disable_span_log_scope_t {
public:
    inline disable_span_log_scope_t();
    inline ~disable_span_log_scope_t();
};

class trace_push_scope_t {
public:
    inline
    trace_push_scope_t() = default;

    inline
    trace_push_scope_t(std::string annotation, std::string rpc_name);

    // No-log ctor
    inline
    trace_push_scope_t(std::string rpc_name);

    inline void
    push_new(std::string annotation, std::string rpc_name);

    inline void
    push(std::string annotation, std::string rpc_name);

    inline void
    push(std::string rpc_name);

    inline
    ~trace_push_scope_t();
private:
    std::unique_ptr<attribute_scope_t> attr_scope;
};

class trace_restore_scope_t {
public:
    inline
    trace_restore_scope_t() = default;

    inline
    trace_restore_scope_t(std::string annotation, std::string rpc_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id);

    inline
    trace_restore_scope_t(std::string annotation, span_ptr_t span);

    inline
    trace_restore_scope_t(span_ptr_t span);

    explicit inline
    trace_restore_scope_t(std::nullptr_t);

    inline void
    restore(std::string annotation, span_ptr_t span);

    inline void
    restore(span_ptr_t span);

    inline void
    restore(std::string annotation, std::string rpc_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id);

    inline
    ~trace_restore_scope_t();

    inline
    void
    pop();
private:
    span_ptr_t old_span;
    std::unique_ptr<attribute_scope_t> attr_scope;
};


template<class... Args>
auto
bind(std::string message, Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>;

template<class... Args>
auto
bind(Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>;

template<class Method>
auto
mem_fn(const char* message, Method m) -> callable_wrapper_t<decltype(std::mem_fn(std::forward<Method>(m)))>;


inline uint64_t generate_id();

class span_t :
    public std::enable_shared_from_this<span_t>
{
public:
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
    blackhole::attribute::set_t
    attributes() const;

    static
    inline
    uint64_t
    cur_time(){
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    inline
    bool
    should_log() const {
        return log_enabled;
    }
private:
    span_t() :
        trace_id(),
        span_id(),
        start_time_us(),
        last_time_us(),
        log_enabled(true),
        parent(nullptr),
        name()
    {}

    span_t(std::string _name, uint64_t _trace_id, uint64_t _span_id, uint64_t _parent_id) :
        trace_id(_trace_id ? _trace_id : generate_id()),
        span_id(_span_id ? _span_id : trace_id),
        start_time_us(cur_time()),
        last_time_us(start_time_us),
        log_enabled(true),
        parent(new span_t("", trace_id, _parent_id)),
        name(_name)
    {}


    span_t(std::string _name, std::shared_ptr<span_t> _parent) :
        trace_id(_parent->empty() ? generate_id() : _parent->trace_id),
        span_id(_parent->empty() ? trace_id : generate_id()),
        start_time_us(cur_time()),
        last_time_us(start_time_us),
        log_enabled(_parent->log_enabled),
        parent(std::move(_parent)),
        name(_name)
    {}

    inline void
    disable_log() {
        log_enabled = false;
    }

    inline void
    enable_log() {
        log_enabled = true;
    }

    inline bool
    empty() const {
        return span_id == 0;
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

    span_t(std::string _name, uint64_t _trace_id, uint64_t _span_id) :
        trace_id(_trace_id),
        span_id(_span_id),
        start_time_us(cur_time()),
        last_time_us(start_time_us),
        log_enabled(true),
        parent(nullptr),
        name(_name)
    {}

    uint64_t trace_id;
    uint64_t span_id;
    uint64_t start_time_us;
    uint64_t last_time_us;
    bool log_enabled;
    std::shared_ptr<span_t> parent;
    std::string name;

    friend void disable_span_log();
    friend class trace_context_t;
    friend class disable_span_log_scope_t;
    friend class trace_restore_scope_t;
};

}}

#include "cocaine/trace/trace_impl.hpp"