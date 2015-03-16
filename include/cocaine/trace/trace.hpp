#pragma once

#include "cocaine/logging.hpp"
#include "cocaine/forwards.hpp"

#include <blackhole/blackhole.hpp>
#include <blackhole/scoped_attributes.hpp>

#include <boost/thread.hpp>
#include <stdexcept>
#include <chrono>

namespace cocaine { namespace tracer {
    uint64_t generate_id();

    class trace_context_t {
    public:

        ~trace_context_t();

        static void
        push(const char* message);

        static void
        pop();

        static void
        set_logger(logging::logger_t& _log);
    private:
        trace_context_t();

        void
        push_impl(const char* message);

        void
        pop_impl();

        static trace_context_t&
        get_context();

        void
        set_attributes();

        static boost::thread_specific_ptr<trace_context_t> thread_context;
        struct trace_t {
            uint64_t trace_id;
            uint64_t span_id;
            trace_t* parent;
            //uint64_t start_time_posix_ns;
            const char* message;
        };

        static logging::logger_t* default_logger;
        trace_t* current_trace;
        std::unique_ptr<logging::log_t> log;
        std::unique_ptr<blackhole::scoped_attributes_t> attributes;
        template <class F>
        friend class callable_wrapper_t;
    };


    template <class F>
    class callable_wrapper_t {
    public:
        callable_wrapper_t(F&& _f) :
            f(std::move(_f)),
            trace_context(trace_context_t::thread_context.release())
        {
            trace_context->attributes.reset();
        }

        template<class ...Args>
        auto
        operator()(Args&& ...args) -> decltype(std::declval<F>()(args...)) {
            assert(trace_context_t::thread_context.get() == nullptr);
            trace_context->set_attributes();
            trace_context_t::thread_context.reset(trace_context);
            return f(args...);
        }
    private:

        F f;
        trace_context_t* trace_context;
        blackhole::attribute::set_t attributes;
    };

    template<class... Args>
    auto
    make_callable(Args&& ...args) -> callable_wrapper_t<decltype(std::bind(args...))>
    {
        typedef callable_wrapper_t<decltype(std::bind(args...))> Result;
        return Result(std::bind(args...));
    }

}}