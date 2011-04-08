#ifndef YAPPI_PLUGIN_HPP
#define YAPPI_PLUGIN_HPP

#include <string>
#include <map>

#define MAX_SOURCES 10

namespace yappi { namespace plugin {

// Plugins are expected to supply at least one factory function
// to initialize sources, given an uri string. Each factory function
// is responsible to initialize sources of one registered scheme
typedef void* (*factory_fn_t)(const char*);

// Plugin is expected to have an 'initialize' function, which should
// return a pointer to an initialized structure of the following format
struct plugin_info_t {
    unsigned int count;
    struct {
        const char* scheme;
        factory_fn_t factory;
    } sources[MAX_SOURCES];
};

// Plugins are expected to have an initialization function,
// which returns all the necessary plugin info
typedef const plugin_info_t* (*initialize_t)(void);

typedef std::map<std::string, std::string> dict_t;        
        
// Source factory function should return a void pointer to an object
// implementing this interface
class source_t {
    public:
        // This method will be called by the scheduler with specified intervals to
        // fetch the new data for publishing
        virtual dict_t fetch() = 0;
};

}}

#endif
