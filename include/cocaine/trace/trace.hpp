#pragma once

#include "cocaine/logging.hpp"
#include "cocaine/forwards.hpp"

#include <blackhole/blackhole.hpp>
#include <blackhole/scoped_attributes.hpp>

#include <boost/thread.hpp>
#include <stdexcept>
#include <chrono>

#define TRACE_PP_CAT(a, b) TRACE_PP_CAT_I(a, b)
#define TRACE_PP_CAT_I(a, b) TRACE_PP_CAT_II(~, a ## b)
#define TRACE_PP_CAT_II(p, res) res

//TODO: comment
#define TRACE_AUTO(event_name) \
    cocaine::tracer::trace_context_t::scope_t TRACE_PP_CAT(trace_scope, __LINE__);\
    blackhole::scoped_attributes_t TRACE_PP_CAT(tracer_attributes, __LINE__) \
    (cocaine::tracer::trace_context_t::get_logger(), cocaine::tracer::trace_context_t::push(event_name).attributes())

//TODO: comment
#define TRACE_PUSH(event_name) \
    blackhole::scoped_attributes_t TRACE_PP_CAT(tracer_attributes, __LINE__) \
    (cocaine::tracer::trace_context_t::get_logger(), cocaine::tracer::trace_context_t::push(event_name).attributes())

//TODO: comment
#define TRACE_RESTORE(event_name, trace_id, parent_id) \
    blackhole::scoped_attributes_t TRACE_PP_CAT(tracer_attributes, __LINE__) \
    (cocaine::tracer::trace_context_t::get_logger(), cocaine::tracer::trace_context_t::push(event_name, trace_id, parent_id).attributes())

//TODO: comment
#define TRACE_POP() cocaine::tracer::trace_context_t::pop()

//TODO: comment
#define TRACE_MOVE_TO_CALLABLE(...) cocaine::tracer::make_callable(__VA_ARGS__)

#define TRACE_LINKED_CALLABLE(callable, ...) cocaine::tracer::make_linked_callable(callable, __VA_ARGS__)


//#define TRACE_SET(trace_id)

namespace cocaine { namespace tracer {
    uint64_t generate_id();

    class trace_context_t {
    public:
        struct trace_t {
            uint64_t trace_id;
            uint64_t span_id;
            trace_t* parent;
            //uint64_t start_time_posix_ns;
            const char* message;
            blackhole::attribute::set_t
            attributes() const {
                return blackhole::attribute::set_t(
                    {
                        blackhole::attribute::make("trace_id", trace_id),
                        blackhole::attribute::make("span_id", span_id),
                        blackhole::attribute::make("parent_id", parent ? parent->span_id : 0),
                    }
                );
            }
        };
        struct scope_t {
            ~scope_t() {
                trace_context_t::pop();
            }
            scope_t() {}
        };

        ~trace_context_t();

        static const trace_t&
        push(const char* message);

        static const trace_t&
        push(const char* message, uint64_t trace_id, uint64_t parent_id);

        static void
        pop();

        static void
        set_logger(logging::logger_t& _log);

        static logging::logger_t&
        get_logger();
    private:
        trace_context_t();

        const trace_t&
        push_impl(const char* message);

        const trace_t&
        push_impl(const char* message, uint64_t trace_id, uint64_t parent_id);

        void
        pop_impl();

        static trace_context_t&
        get_context();

        static boost::thread_specific_ptr<trace_context_t> thread_context;

        static logging::logger_t* default_logger;

        size_t depth;
        trace_t* current_trace;
        std::unique_ptr<logging::log_t> log;
        template <class F>
        friend class callable_wrapper_t;
    };


    template <class F>
    class callable_wrapper_t {
    public:
        template<class C>
        friend class callable_wrapper_t;

        callable_wrapper_t(F&& _f) :
            f(std::move(_f)),
            trace_context(trace_context_t::thread_context.release()),
            run_barrier(std::make_shared<std::atomic_flag>(true))
        {
        }

        template<class C>
        callable_wrapper_t(callable_wrapper_t<C> other, F&& _f) :
            f(std::move(_f)),
            trace_context(other.trace_context),
            run_barrier(other.run_barrier)
        {}

        template<class ...Args>
        auto
        operator()(Args&& ...args) -> decltype(std::declval<F>()(args...)) {
            bool should_run = !run_barrier->test_and_set();
            if(should_run) {
                assert(trace_context_t::get_context().depth == 0);
                auto t = trace_context->current_trace;
                assert(t);
                blackhole::scoped_attributes_t attr(*trace_context->log,
                    {
                        blackhole::attribute::make("trace_id", t->trace_id),
                        blackhole::attribute::make("span_id", t->span_id),
                        blackhole::attribute::make("parent_id", t->parent ? t->parent->span_id : 0)
                    }
                );
                trace_context_t::thread_context.reset(trace_context);
                trace_context = nullptr;
                auto scope = context_cleaner_t();
                return f(args...);
            }
            else {
                return f(args...);
            }
        }
    private:
        struct context_cleaner_t {
            ~context_cleaner_t() {
                auto& ctx = trace_context_t::get_context();
                while(ctx.depth) {
                    ctx.pop_impl();
                }
            }
            context_cleaner_t() {}
        };
        F f;
        trace_context_t* trace_context;
        std::shared_ptr<std::atomic_flag> run_barrier;
    };

    template<class F, class... Args>
    auto
    make_linked_callable(F& f, Args&& ...args) -> callable_wrapper_t<decltype(std::bind(args...))>
    {
        typedef callable_wrapper_t<decltype(std::bind(args...))> Result;
        return Result(f, std::bind(args...));
    }

    template<class... Args>
    auto
    make_callable(Args&& ...args) -> callable_wrapper_t<decltype(std::bind(args...))>
    {
        typedef callable_wrapper_t<decltype(std::bind(args...))> Result;
        return Result(std::bind(args...));
    }

}}