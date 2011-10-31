#include "plugin.hpp"

namespace cocaine { namespace plugin {

class plugin_t:
    public source_t
{
    public:
        static source_t* create(const std::string& args) {
            return new plugin_t;
        }

    public:
        plugin_t(const std::string& args) {
            // Your code to initialize the plugin instance
        }
    
        virtual void invoke(
            callback_fn_t callback,
            const std::string& method,
            const void* request = NULL,
            size_t request_size = 0)
        {
            // Your code to invoke the specified method
        }
};

static const source_info_t plugin_info[] = {
    { "plugin", &plugin_t::create },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        return plugin_info;
    }

    // __attribute__((destructor)) void finalize() {
        // This is guaranteed to be called from the main thread,
        // when there're no more plugin instances left running
    // }
}

}}
