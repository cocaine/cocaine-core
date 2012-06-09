#include <fstream>

#include <zmq.hpp>

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

    cocaine::config_t config("tests/library_config.json");

    std::cout << "=== Configured ===" << std::endl;

    cocaine::context_t context(
        config,
        boost::make_shared<stdio_sink_t>()
    );

    std::cout << "=== Context created ===" << std::endl;

    cocaine::app_t app(context, "my_app@1");

    std::cout << "=== App created ===" << std::endl;
    
    app.start();
    
    std::cout << "=== App started ===" << std::endl;
    
    for(int i = 0; i < 10; i++) {
        app.enqueue(
            boost::make_shared<my_job_t>(
                "rimz_func",
                cocaine::blob_t(data, 5)
            )
        );
    }

    std::cout << "=== Jobs enqueued while running ===" << std::endl;
    
    Json::Value state(app.info());
    std::cout << state;

    sleep(1);

    app.stop();

    std::cout << "=== App stopped ===" << std::endl;
    
    for(int i = 0; i < 10; i++) {
        app.enqueue(
            boost::make_shared<my_job_t>(
                "rimz_func",
                cocaine::blob_t(data, 5)
            )
        );
    }

    std::cout << "=== Jobs enqueued while stopped ===" << std::endl;
    
    sleep(5);
    
    return 0;
}
