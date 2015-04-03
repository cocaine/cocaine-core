#include "cocaine/trace/trace_new.hpp"
//#pragma once
//
//#include "cocaine/logging.hpp"
//#include "cocaine/forwards.hpp"
//
//#include <blackhole/blackhole.hpp>
//#include <blackhole/scoped_attributes.hpp>
//
//#include <boost/thread.hpp>
//#include <stdexcept>
//#include <chrono>
//
//#define TRACE_PP_CAT(a, b) TRACE_PP_CAT_I(a, b)
//#define TRACE_PP_CAT_I(a, b) TRACE_PP_CAT_II(~, a ## b)
//#define TRACE_PP_CAT_II(p, res) res
//
//
////TODO: comment
//#define TRACE_RESTORE(event_name, trace_id, parent_id) \
//    cocaine::tracer::trace_context_t::scope_t TRACE_PP_CAT(trace_scope, __LINE__);\
//    cocaine::tracer::trace_context_t::reset(event_name, trace_id, parent_id)
//
////TODO: comment
//#define TRACE_POP() cocaine::tracer::trace_context_t::pop()
//
////TODO: comment
//#define TRACE_CALLABLE(name, ...) cocaine::tracer::make_callable(name, __VA_ARGS__)
//
//
////#define TRACE_SET(trace_id)
//
//namespace cocaine { namespace tracer {
//    inline uint64_t generate_id();
//
//
//    class trace_context_t {
//    public:
//        struct trace_t :
//            public std::enable_shared_from_this<trace_t>
//        {
//            trace_t() :
//                start_time_posix_ns(std::chrono::duration_cast<std::chrono::milliseconds>(
//                    std::chrono::system_clock::now().time_since_epoch()).count())
//            {}
//            uint64_t trace_id;
//            uint64_t span_id;
//            std::shared_ptr<trace_t> parent;
//            long long start_time_posix_ns;
//            const char* message;
//            const char* service_name;
//
//            uint64_t get_parent_id() {
//                return parent ? parent->span_id : 0;
//            }
//
//            inline
//            blackhole::attribute::set_t
//            attributes() const {
//                return blackhole::attribute::set_t(
//                    {
//                        blackhole::attribute::make("trace_id", trace_id),
//                        blackhole::attribute::make("span_id", span_id),
//                        blackhole::attribute::make("parent_id", parent ? parent->span_id : 0),
//                    }
//                );
//            }
//        };
//        struct scope_t {
//            inline
//            ~scope_t() {
//                TRACE_POP();
//            }
//
//            inline
//            scope_t() {}
//        };
//
//        inline
//        ~trace_context_t();
//
//        inline
//        static const trace_t&
//        push(const char* message);
//
//        inline
//        static void
//        set_message(const char* message) {
//            get_context().current_trace->message = message;
//            COCAINE_LOG_INFO(get_context().log, "Trace: %s", message)("trace_event", "start");
//        }
//
//        inline
//        static void
//        set_service_name(const char* service_name) {
//            get_context().current_trace->service_name = service_name;
//        }
//
//        inline
//        static const trace_t&
//        reset(const char* message, uint64_t trace_id, uint64_t parent_id);
//
//        inline
//        static void
//        pop();
//
//        inline
//        static void
//        set_logger(logging::logger_t& _log);
//
//        inline
//        static logging::logger_t&
//        get_logger();
//
//        inline
//        static std::string&
//        service_name() {
//            static std::string s_name;
//            return s_name;
//        }
//    private:
//        inline
//        trace_context_t();
//
//        inline
//        trace_context_t(const trace_context_t& other);
//
//        inline
//        const trace_t&
//        push_impl(const char* message);
//
//        inline
//        const trace_t&
//        reset_impl(const char* message, uint64_t trace_id, uint64_t parent_id);
//
//        inline
//        void
//        pop_impl();
//
//        inline
//        static trace_context_t&
//        get_context();
//
//        inline
//        static boost::thread_specific_ptr<trace_context_t>& thread_context() {
//            static boost::thread_specific_ptr<trace_context_t> ctx;
//            return ctx;
//        }
//
//        inline
//        static logging::logger_t*& default_logger() {
//            static logging::logger_t* log;
//            return log;
//        }
//
//        size_t depth;
//        std::vector<std::unique_ptr<blackhole::scoped_attributes_t>> attributes;
//        std::shared_ptr<trace_t> current_trace;
//        std::unique_ptr<logging::log_t> log;
//        template <class F>
//        friend class callable_wrapper_t;
//    };
//
//
//    //TODO: Move out simple case
//    template <class F>
//    class callable_wrapper_t {
//    public:
//        template<class C>
//        friend class callable_wrapper_t;
//
//        inline
//        callable_wrapper_t(const char* _message, F&& _f) :
//            f(std::move(_f)),
//            trace_context(trace_context_t::get_context())
//        {
//            trace_context.set_message(message);
//            if(trace_context.current_trace != nullptr) {
//                trace_context.push_impl(message);
//                //keep attribute stack in current thread in order.
//                trace_context.attributes.pop_back();
//            }
//        }
//        struct cleanup_t {
//
//            cleanup_t(trace_context_t* _old_context) :
//                old_context(_old_context)
//            {}
//
//            ~cleanup_t() {
//                trace_context_t::thread_context().reset(old_context);
//            }
//
//            trace_context_t* old_context;
//        };
//
//        template<class ...Args>
//        auto
//        operator()(Args&& ...args) -> decltype(std::declval<F>()(args...)) {
//            if(!trace_context.current_trace) {
//                return f(std::forward<Args>(args)...);
//            }
//            cleanup_t c(trace_context_t::thread_context().release());
//            trace_context_t::thread_context().reset(new trace_context_t(trace_context));
//            auto& ctx = trace_context_t::get_context();
//            const auto& ct = ctx.current_trace;
//            trace_context_t::scope_t outer_scope;
//            ctx.attributes.emplace_back(new blackhole::scoped_attributes_t(*ctx.log, ct->attributes()));
//            return f(std::forward<Args>(args)...);
//        }
//    private:
//        struct context_cleaner_t {
//            inline
//            ~context_cleaner_t() {
//                auto& ctx = trace_context_t::get_context();
//                while(ctx.depth) {
//                    ctx.pop_impl();
//                }
//            }
//            inline
//            context_cleaner_t() {}
//        };
//        const char* message;
//        F f;
//        trace_context_t trace_context;
//    };
//
//    template<class... Args>
//    auto
//    make_callable(const char* message, Args&& ...args) -> callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))>
//    {
//        typedef callable_wrapper_t<decltype(std::bind(std::forward<Args>(args)...))> Result;
//        return Result(message, std::bind(std::forward<Args>(args)...));
//    }
//
//uint64_t generate_id() {
//    static std::atomic_ulong counter(0);
//    return counter++;
//}
//
//trace_context_t&
//trace_context_t::get_context() {
//    if(thread_context().get() == nullptr) {
//        thread_context().reset(new trace_context_t());
//    }
//    return *thread_context().get();
//}
//
//const trace_context_t::trace_t&
//trace_context_t::push(const char* message) {
//    auto& ctx = get_context();
//    return ctx.push_impl(message);
//}
//
//const trace_context_t::trace_t&
//trace_context_t::reset(const char* message, uint64_t trace_id, uint64_t parent_id) {
//    thread_context().reset(nullptr);
//    auto& ctx = get_context();
//    return ctx.reset_impl(message, trace_id, parent_id);
//}
//
//const trace_context_t::trace_t&
//trace_context_t::reset_impl(const char* message, uint64_t trace_id, uint64_t parent_id) {
//    assert(current_trace == nullptr);
//    uint64_t new_id = generate_id();
//    if(trace_id == 0) {
//        trace_id = new_id;
//    }
//    current_trace = std::make_shared<trace_t>();
//
//
//    current_trace->message = message;
//    current_trace->trace_id = trace_id;
//    current_trace->span_id = new_id;
//    if(parent_id != 0) {
//        current_trace->parent = std::make_shared<trace_t>();
//        current_trace->parent->trace_id = trace_id;
//        current_trace->parent->span_id = parent_id;
//    }
//    current_trace->send("start");
//
//    attributes.emplace_back(new blackhole::scoped_attributes_t(*log, current_trace->attributes()));
//    COCAINE_LOG_INFO(log, "Trace: %s", message)("trace_event", "start");
//
//    return *current_trace;
//}
//
//const trace_context_t::trace_t&
//trace_context_t::push_impl(const char* message) {
////    std::cerr << "PUSH_IMPL. THIS: " << this << ", thread:" << std::this_thread::get_id() << std::endl;
//    depth++;
//    assert(log);
//    auto result = std::make_shared<trace_t>();
////    std::cerr << "NEW: " << result << std::endl;
//    result->message = message;
//    uint64_t new_id = generate_id();
//    result->parent = current_trace;
//    if(current_trace.use_count() == 0) {
//        result->trace_id = new_id;
//        result->span_id = new_id;
//    }
//    else {
//        result->trace_id = current_trace->trace_id;
//        result->span_id = new_id;
//    }
//    current_trace = result;
//    current_trace->send("start");
//    attributes.emplace_back(new blackhole::scoped_attributes_t(*log, current_trace->attributes()));
//    COCAINE_LOG_INFO(log, "Trace: %s", message)("trace_event", "start");
//    return *current_trace;
//}
//
////Implementation. Required for CNF because it is not linked with libcocaine-core
//
//void
//trace_context_t::pop() {
//    auto& ctx = get_context();
//    ctx.pop_impl();
//}
//
//void
//trace_context_t::pop_impl() {
////    std::cerr << "POP_IMPL. THIS: " << this << ", thread:" << std::this_thread::get_id() << std::endl;
//    if(depth == 0) {
//        return;
//    }
//    depth--;
//    assert(log);
//    assert(current_trace);
//    auto ptr = current_trace;
//    current_trace = current_trace->parent;
//    ptr->send("stop");
//    COCAINE_LOG_INFO(log, "Trace: %s", ptr->message)("trace_event", "stop");
//    attributes.pop_back();
////    std::cerr << "DELETE: " << ptr << std::endl;
//}
//
//trace_context_t::trace_context_t() :
//    depth(0),
//    current_trace(nullptr),
//    log(new logging::log_t(get_logger(), {}))
//{
////    std::cerr << "CONTEXT CTOR. THIS: " << this << ", thread:" << std::this_thread::get_id() << std::endl;
//}
//
//trace_context_t::trace_context_t(const trace_context_t& other) :
//    depth(other.depth),
//    //Do not copy attributes. They are thread local.
//    attributes(),
//    current_trace(other.current_trace),
//    log(new logging::log_t(get_logger(), {}))
//{
//}
//
//trace_context_t::~trace_context_t() {
//}
//
//void
//trace_context_t::set_logger(logging::logger_t& _log) {
//    default_logger() = &_log;
//}
//
//logging::logger_t&
//trace_context_t::get_logger() {
//    assert(default_logger());
//    return *default_logger();
//}
//}}
