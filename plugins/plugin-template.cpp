#include <stdexcept>

#include "plugin.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error

namespace yappi { namespace plugin {

class plugin_t: public source_t {
    public:
        plugin_t(const std::string& uri):
            source_t(uri)
        {
            // Your code to initialize the plugin instance
        }
    
        virtual dict_t fetch() {
            dict_t dict;

            // Your code to get the job done
            
            return dict;
        }
};

source_t* create_plugin_instance(const char* uri) {
    return new plugin_t(uri);
}

static const plugin_info_t plugin_info = {
    1,
    {
        { "plugin", &create_plugin_instance }
    }
};

extern "C" {
    const plugin_info_t* initialize() {
        // Global initialization logic
        // This function will be called once, from the main thread

        return &plugin_info;
    }

    // __attribute__((destructor)) void finalize() {
        // This is guaranteed to be called from the main thread,
        // when there're no more plugin instances left running
    // }
}

}}
