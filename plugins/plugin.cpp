#include <stdexcept>

#include "plugin.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error
// * std::invalid_argument

class plugin_t: public source_t {
    public:
        plugin_t(const std::string& uri) {
            // Your code to initialize the plugin instance
        }
    
        virtual dict_t fetch() {
            dict_t dict;

            // Your code to get the job done
            
            return dict;
        }
};

void* create_plugin_instance(const char* uri) {
    return new plugin_t(uri);
}

static const plugin_info_t plugin_info = {
    1,
    {
        { "plugin", &create_plugin_instance }
    }
};

extern "C" {
    const plugin_info_t* get_plugin_info() {
        return &plugin_info;
    }
}
