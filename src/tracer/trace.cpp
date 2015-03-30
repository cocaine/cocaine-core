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

const trace_context_t::trace_t&
trace_context_t::push(const char* message) {
    auto& ctx = get_context();
    return ctx.push_impl(message);
}

const trace_context_t::trace_t&
trace_context_t::push(const char* message, uint64_t trace_id, uint64_t parent_id) {
    auto& ctx = get_context();
    return ctx.push_impl(message, trace_id, parent_id);
}

const trace_context_t::trace_t&
trace_context_t::push_impl(const char* message, uint64_t trace_id, uint64_t parent_id) {
    assert(current_trace == nullptr);
    if(trace_id == 0) {
        trace_id = generate_id();
    }
    current_trace = new trace_t();
    uint64_t new_id = generate_id();

    current_trace->message = message;
    current_trace->trace_id = trace_id;
    current_trace->span_id = new_id;
    if(parent_id != 0) {
        current_trace->parent = new trace_t();
        current_trace->parent->trace_id = trace_id;
        current_trace->parent->span_id = parent_id;
    }

    blackhole::scoped_attributes_t scope(*log, current_trace->attributes());
    COCAINE_LOG_INFO(log, "Trace: %s", message)("event", "start");
    return *current_trace;
}

const trace_context_t::trace_t&
trace_context_t::push_impl(const char* message) {
//    std::cerr << "PUSH_IMPL. THIS: " << this << ", thread:" << std::this_thread::get_id() << std::endl;
    depth++;
    assert(log);
    trace_t* result = new trace_t();
//    std::cerr << "NEW: " << result << std::endl;
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
    blackhole::scoped_attributes_t scope(*log, current_trace->attributes());
    COCAINE_LOG_INFO(log, "Trace: %s", message)("event", "start");
    return *current_trace;
}

void
trace_context_t::pop() {
    auto& ctx = get_context();
    ctx.pop_impl();
}

void
trace_context_t::pop_impl() {
//    std::cerr << "POP_IMPL. THIS: " << this << ", thread:" << std::this_thread::get_id() << std::endl;
    if(depth == 0) {
        return;
    }
    depth--;
    assert(log);
    assert(current_trace);
    auto ptr = current_trace;
    current_trace = current_trace->parent;

    COCAINE_LOG_INFO(log, "Trace: %s", ptr->message)("event", "stop");
//    std::cerr << "DELETE: " << ptr << std::endl;
    delete ptr;
}

trace_context_t::trace_context_t() :
    depth(0),
    current_trace(nullptr)
{
//    std::cerr << "CONTEXT CTOR. THIS: " << this << ", thread:" << std::this_thread::get_id() << std::endl;
}

trace_context_t::~trace_context_t() {
//    std::cerr << "CONTEXT DTOR. THIS: " << this << ", thread:" << std::this_thread::get_id() << std::endl;
    while(current_trace != nullptr) {
        pop_impl();
    }
//    std::cerr << "CONTEXT DTOR END. THIS: " << this << ", thread:" << std::this_thread::get_id() << std::endl;
}

void
trace_context_t::set_logger(logging::logger_t& _log) {
    default_logger = &_log;
}

logging::logger_t&
trace_context_t::get_logger() {
    assert(default_logger);
    return *default_logger;
}

}}