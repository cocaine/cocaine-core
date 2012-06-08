#include <fstream>

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/job.hpp"

using namespace cocaine;

class stdio_sink_t:
    public cocaine::logging::sink_t
{
    public:
        stdio_sink_t():
            cocaine::logging::sink_t(cocaine::logging::debug)
        { }

        virtual void emit(cocaine::logging::priorities, const std::string& message) const {
            std::cout << message << std::endl;
        }
};

class my_job_t:
    public cocaine::engine::job_t
{
    public:
        my_job_t(const std::string& event, const cocaine::blob_t& blob):
            cocaine::engine::job_t(event, blob)
        { }

    public:
        virtual void react(const cocaine::engine::events::chunk& event) {
            std::cout << "chunk: " << std::string((const char*)event.message.data(), event.message.size()) << std::endl;
        }
        
        virtual void react(const cocaine::engine::events::choke& event) {
            std::cout << "choke" << std::endl;
        }
        
        virtual void react(const cocaine::engine::events::error& event) {
            std::cout << "error: " << event.message << std::endl;
        }
};

int main() {
    const char data[] = "data";
    const char abc[] = "123123123123";

    cocaine::config_t config("tests/library_config.json");

    std::cout << "Configured" << std::endl;

    cocaine::context_t context(
        config,
        boost::make_shared<stdio_sink_t>()
    );

    std::cout << "Created context" << std::endl;

    Json::Reader reader;
    Json::Value manifest;

    std::fstream m("for-zbr/package/manifest.json");

    reader.parse(m, manifest);

    std::fstream p("for-zbr/package/code.tar.gz", std::fstream::binary | std::fstream::in);

    std::stringstream ss;
    ss << p.rdbuf();

    storages::objects::value_type v = {
        manifest,
        storages::objects::data_type(
            ss.str().data(),
            ss.str().size()
        )
    };

    context.storage<storages::objects>("core")->put("apps", "my_app@1", v);

    std::cout << "Stored" << std::endl;

    cocaine::app_t app(context, "my_app@1");

    std::cout << "Created app" << std::endl;
    
    app.start();
    
    std::cout << "Engine started" << std::endl;
    
    for(int i = 0; i < 10; i++) {
        app.enqueue(
            boost::make_shared<my_job_t>(
                "rimz_func",
                cocaine::blob_t(data, 5)
            )
        );
    }

    Json::Value state(app.info());
    
    std::cout << state;

    sleep(1);

    app.stop();

    for(int i = 0; i < 10; i++) {
        app.enqueue(
            boost::make_shared<my_job_t>(
                "rimz_func",
                cocaine::blob_t(data, 5)
            )
        );
    }

    return 0;
}
