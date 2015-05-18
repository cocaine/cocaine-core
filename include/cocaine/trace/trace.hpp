/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@me.com>
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

    You should have received a copy of the GNU Lesser Gene ral Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_TRACER_TRACER_HPP
#define COCAINE_TRACER_TRACER_HPP
#include <blackhole/scoped_attributes.hpp>
#include "cocaine/forwards.hpp"

#include <random>

namespace cocaine { namespace tracer {

class attribute_scope_t;
class span_t;
class logger_t;

class new_trace_scope_t;
class trace_reset_scope_t;
class trace_push_scope_t;
class trace_restore_scope_t;

template <class F>
class callable_wrapper_t;

typedef std::shared_ptr<span_t> span_ptr_t;

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

//Used to create a new trace. For example in client application.
class new_trace_scope_t {
public:
    inline
    new_trace_scope_t(std::string rpc_name);

    inline
    ~new_trace_scope_t();
private:
    std::unique_ptr<attribute_scope_t> attr_scope;
};

//Used to reset current trace if further tracing is undesirable - for example in logging in CNF
class trace_reset_scope_t {
public:
    inline
    trace_reset_scope_t();

    inline
    ~trace_reset_scope_t();

private:
    span_ptr_t old_span;
};

//Used to push a span to trace tree. Used just before making sub-RPC
//Also holds an attribute_scope which can be used to set up some scope attributes - for example blackhole scoped attributes.
class trace_push_scope_t {
public:
    // Push span
    inline
    trace_push_scope_t(std::string rpc_name);

    // Push span and log an anotation via tracer::log
    inline
    trace_push_scope_t(std::string annotation, std::string rpc_name);

    inline
    ~trace_push_scope_t();
private:
    std::unique_ptr<attribute_scope_t> attr_scope;
};

// Restores trace from some external source.
// For example when message was read from network or
// we executing some async operation with previously stored span
// Can be default constructed and restored later only once OR non default constructed.
// calling restore after non default ctor or previous restore is misusage.
// When object of this type goes out of scope it restores original trace
class trace_restore_scope_t {
public:
    inline
    trace_restore_scope_t() = default;

    // Restores trace from trace-span-parent param. Logs event via logger.
    inline
    trace_restore_scope_t(std::string annotation, std::string rpc_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id);

    // Restores trace from saved span. Logs annotation via logger.
    inline
    trace_restore_scope_t(std::string annotation, span_ptr_t span);

    // Restores trace from saved span. No-log.
    inline
    trace_restore_scope_t(span_ptr_t span);

    // Restores trace from saved span. Logs annotation via logger.
    inline
    void
    restore(std::string annotation, span_ptr_t span);

    // Restores trace from saved span. No-log.
    inline
    void
    restore(span_ptr_t span);

    // Restores trace from trace-span-parent param. Logs event via logger.
    inline
    void
    restore(std::string annotation, std::string rpc_name, uint64_t trace_id, uint64_t span_id, uint64_t parent_id);

    inline
    ~trace_restore_scope_t();

private:
    span_ptr_t old_span;
    std::unique_ptr<attribute_scope_t> attr_scope;
};

//Bind callback to store trace and log now and on callback call.
template<class... Args>
auto
bind(std::string message, Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>;

//Bind callback to store trace. Behaves exactly as std::bind
template<class... Args>
auto
bind(Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>;

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
    empty() const {
        return span_id == 0;
    }

    inline
    const std::string&
    get_name() const {
        return name;
    }

private:
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
        parent(nullptr),
        name(_name)
    {}

    uint64_t trace_id;
    uint64_t span_id;
    uint64_t start_time_us;
    uint64_t last_time_us;
    std::shared_ptr<span_t> parent;
    std::string name;

    friend class trace_context_t;
    friend class trace_reset_scope_t;
};

}}

#include "cocaine/trace/trace_impl.hpp"

#endif
