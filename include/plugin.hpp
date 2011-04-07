#ifndef YAPPI_PLUGIN_HPP
#define YAPPI_PLUGIN_HPP

#include <string>
#include <map>

#define MAX_FACTORIES 10

namespace yappi { namespace plugin {

// Plugins are expected to supply at least one factory function
// to initialize sources, given an uri string. Each factory function
// is responsible to initialize sources of one registered scheme
typedef void* (*factory_t)(const char*);

// Plugin is expected to have an 'initialize' function, which should
// return a pointer to an initialized structure of the following format
struct plugin_info_t {
    unsigned int count;
    struct {
        const char* scheme;
        factory_t factory;
    } factories[MAX_FACTORIES];
};

// Source factory function should return a void pointer to an object
// implementing this interface
class source_t {
    public:
        typedef std::map<std::string, std::string> dict_t;        
        
        // This method will be called by a scheduler with specified intervals
        // for specified amount of time. The scheduler will wait for this
        // method to return, until the specified timeout expires, and then the engine
        // will be killed.
        virtual dict_t fetch() = 0;
};

}}

#endif
