#include "cocaine/common.hpp"

#include "cocaine/api/connect.hpp"
#include "cocaine/api/resolve.hpp"
#include "cocaine/api/service.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/chamber.hpp"
#include "cocaine/detail/engine.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/rpc/dispatch.hpp"

#include <random>

#include <celero/Celero.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace cocaine { namespace io {

// Test API

struct test_tag;

struct test {
    struct mute_slot {
        typedef test_tag tag;

        static const char* alias() {
            return "mute_slot";
        }

        typedef boost::mpl::list<
            std::string
        > argument_type;

        typedef void upstream_type;
    };

    struct void_slot {
        typedef test_tag tag;

        static const char* alias() {
            return "void_slot";
        }

        typedef boost::mpl::list<
            std::string
        > argument_type;
    };

    struct echo_slot {
        typedef test_tag tag;

        static const char* alias() {
            return "echo_slot";
        }

        typedef boost::mpl::list<
            std::string
        > argument_type;

        typedef option_of<
            std::string
        >::tag upstream_type;
    };
};

template<>
struct protocol<test_tag> {
    typedef boost::mpl::int_<
        1
    > version;

    typedef boost::mpl::list<
        test::mute_slot,
        test::void_slot,
        test::echo_slot
    > messages;

    typedef test scope;
};

} // namespace io

struct test_service_t:
    public dispatch<io::test_tag>
{
    test_service_t():
        dispatch<io::test_tag>("benchmark")
    {
        using namespace std::placeholders;

        on<io::test::mute_slot>(std::bind(&test_service_t::on_mute_slot, this, _1));
        on<io::test::void_slot>(std::bind(&test_service_t::on_void_slot, this, _1));
        on<io::test::echo_slot>(std::bind(&test_service_t::on_echo_slot, this, _1));
    }

    void
    on_mute_slot(const std::string& COCAINE_UNUSED_(input)) {
        return;
    }

    void
    on_void_slot(const std::string& COCAINE_UNUSED_(input)) {
        return;
    }

    std::string
    on_echo_slot(const std::string& input) {
        return input;
    }
};

} // namespace cocaine

struct test_globals_t {
    test_globals_t() {
        std::random_device rd;

        std::generate_n(std::back_inserter(data1K),  1024,  std::ref(rd));
        std::generate_n(std::back_inserter(data8K),  8192,  std::ref(rd));
        std::generate_n(std::back_inserter(data65K), 65536, std::ref(rd));
    }

    std::string data1K, data8K, data65K;
};

static
const test_globals_t&
globals() {
    static const test_globals_t instance;
    return instance;
}

struct test_fixture_t:
    public celero::TestFixture
{
    std::unique_ptr<cocaine::context_t> context;
    std::unique_ptr<boost::asio::io_service> reactor;
    std::unique_ptr<boost::thread> chamber;

    cocaine::api::client<cocaine::io::test_tag> service;

public:
    virtual
    void
    setUp(int64_t) {
        context.reset(new cocaine::context_t(cocaine::config_t("cocaine-benchmark.conf"), "core"));
        reactor.reset(new boost::asio::io_service());

        context->insert("benchmark", std::make_unique<cocaine::actor_t>(
           *context,
            std::make_shared<boost::asio::io_service>(),
            std::make_unique<cocaine::test_service_t>()
        ));

        auto endpoints = context->locate("benchmark").get().endpoints();
        auto socket = std::make_unique<boost::asio::ip::tcp::socket>(*reactor);

        boost::asio::connect(*socket, endpoints.begin(), endpoints.end());

        service.connect(std::move(socket));
        chamber.reset(new boost::thread([this]{ reactor->run(); }));
    }

    virtual
    void
    tearDown() {
        reactor->stop();
        context->remove("benchmark");
        chamber->join();
    }
};

BASELINE_F (ClientIoBenchmark1K,  MuteSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::mute_slot>(nullptr, globals().data1K);
}

BENCHMARK_F(ClientIoBenchmark1K,  VoidSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::void_slot>(nullptr, globals().data1K);
}

BENCHMARK_F(ClientIoBenchmark1K,  EchoSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::echo_slot>(nullptr, globals().data1K);
}

BASELINE_F (ClientIoBenchmark8K,  MuteSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::mute_slot>(nullptr, globals().data8K);
}

BENCHMARK_F(ClientIoBenchmark8K,  VoidSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::void_slot>(nullptr, globals().data8K);
}

BENCHMARK_F(ClientIoBenchmark8K,  EchoSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::echo_slot>(nullptr, globals().data8K);
}

BASELINE_F (ClientIoBenchmark65K, MuteSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::mute_slot>(nullptr, globals().data65K);
}

BENCHMARK_F(ClientIoBenchmark65K, VoidSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::void_slot>(nullptr, globals().data65K);
}

BENCHMARK_F(ClientIoBenchmark65K, EchoSlot, test_fixture_t, 10, 100000) {
    service.invoke<cocaine::io::test::echo_slot>(nullptr, globals().data65K);
}

CELERO_MAIN
