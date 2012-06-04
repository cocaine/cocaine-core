#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/engine.hpp"
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

    std::cout << "Configured" << std::endl;

    cocaine::context_t context(
        config,
        boost::make_shared<stdio_sink_t>()
    );

    std::cout << "Created context" << std::endl;
    
    cocaine::engine::engine_t engine(context, "rimz_app@1");

    std::cout << "Created engine" << std::endl;
    
    engine.start();
    
    std::cout << "Engine started" << std::endl;
    
    for(int i = 0; i < 10; i++) {
        engine.enqueue(
            boost::make_shared<my_job_t>(
                "rimz_func",
                cocaine::blob_t(data, 5)
            )
        );
    }

    Json::Value state(engine.info());
    
    std::cout << state;

    sleep(1);

    engine.stop();

    for(int i = 0; i < 10; i++) {
        engine.enqueue(
            boost::make_shared<my_job_t>(
                "rimz_func",
                cocaine::blob_t(data, 5)
            )
        );
    }

    return 0;
}
