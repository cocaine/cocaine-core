#include <cocaine/common.hpp>
#include <cocaine/forwards.hpp>
#include <cocaine/rpc/protocol.hpp>

namespace cocaine {
namespace io {

struct test_tag;
struct test_transition_tag;

struct test {
    struct method1 {
        typedef test_tag tag;
        static const char* alias() { return "method1"; }
        typedef boost::mpl::list<>::type argument_type;
        typedef void upstream_type;
    };

    struct method2 {
        typedef test_tag tag;
        static const char* alias() { return "method2"; }
        typedef boost::mpl::list<>::type argument_type;
        typedef test_transition_tag dispatch_type;
        typedef test_transition_tag upstream_type;
    };
};

struct inner_test {
    struct inner_method1 {
        typedef test_transition_tag tag;
        static const char* alias() { return "inner_method1"; }
        typedef boost::mpl::list<>::type argument_type;
        typedef void upstream_type;
    };
};

template<>
struct protocol<test_tag> {
    typedef boost::mpl::int_<1>::type version;
    typedef boost::mpl::list<test::method1, test::method2>::type messages;
    typedef test scope;
};

template<>
struct protocol<test_transition_tag> {
    typedef boost::mpl::int_<1>::type version;
    typedef boost::mpl::list<inner_test::inner_method1>::type messages;
    typedef test_transition_tag transition_type;
    typedef inner_test scope;
};

} // namespace io
} // namespace cocaine
