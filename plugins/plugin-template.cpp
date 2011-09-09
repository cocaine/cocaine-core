#include <stdexcept>

#include "plugin.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error

namespace yappi { namespace plugin {

class plugin_t:
    public source_t
{
    public:
        plugin_t(const std::string& uri):
            source_t(uri)
        {
            // Your code to initialize the plugin instance
        }
    
        virtual uint32_t capabilities() {
            return NONE;
        }
};

source_t* create_plugin_instance(const char* uri) {
    return new plugin_t(uri);
}

static const source_info_t plugin_info[] = {
    { "plugin", &create_plugin_instance },
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
