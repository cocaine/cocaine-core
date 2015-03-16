#include "cocaine/trace/trace.hpp"

namespace cocaine { namespace tracer {

uint64_t generate_id() {
    return rand();
}

boost::thread_specific_ptr<trace_context_t> trace_context_t::thread_context;
logging::logger_t* trace_context_t::default_logger = nullptr;

trace_context_t&
trace_context_t::get_context() {
    if(thread_context.get() == nullptr) {
        thread_context.reset(new trace_context_t());
        assert(default_logger);
        thread_context->log.reset(new logging::log_t(*default_logger, {}));
    }
    return *thread_context.get();
}

void
trace_context_t::push(const char* message) {
    auto& ctx = get_context();
    ctx.push_impl(message);
}

void
trace_context_t::push_impl(const char* message) {
    assert(log);
    trace_t* result = new trace_t();
    result->message = message;
    uint64_t new_id = generate_id();
    result->parent = current_trace;
    if(current_trace == nullptr) {
        result->trace_id = new_id;
        result->span_id = new_id;
    }
    else {
        result->trace_id = current_trace->trace_id;
        result->span_id = new_id;
    }
    current_trace = result;
    set_attributes();
    COCAINE_LOG_INFO(log, "Trace: %s", message)("event", "start");
}

void
trace_context_t::pop() {
    auto& ctx = get_context();
    ctx.pop_impl();
}

void
trace_context_t::pop_impl() {
    assert(log);
    assert(current_trace);
    auto ptr = current_trace;
    current_trace = current_trace->parent;
    delete ptr;
    COCAINE_LOG_INFO(log, "Trace: %s", ptr->message)("event", "stop");
    set_attributes();
}

trace_context_t::trace_context_t() {}

trace_context_t::~trace_context_t() {
    while(current_trace != nullptr) {
        pop_impl();
    }
}

void
trace_context_t::set_logger(logging::logger_t& _log) {
    default_logger = &_log;
}
void
trace_context_t::set_attributes() {
    attributes.reset(new blackhole::scoped_attributes_t(*log,
        {
            blackhole::attribute::make("trace_id", current_trace->trace_id),
            blackhole::attribute::make("span_id", current_trace->span_id),
            blackhole::attribute::make("parent_id", current_trace->parent ? current_trace->parent->span_id : 0)
        }
    ));
}
}}