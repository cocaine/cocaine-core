#ifndef YAPPI_PLUGIN_HPP
#define YAPPI_PLUGIN_HPP

#include <string>
#include <map>

#define MAX_FACTORIES 10

namespace yappi { namespace plugins {

typedef void* (*factory_fn_t)(const char*);

struct plugin_info_t {
    unsigned int count;
    struct {
        const char* scheme;
        factory_fn_t factory;
    } factories[MAX_FACTORIES];
};

typedef std::map<std::string, std::string> dict_t;        

class source_t {
    public:
        virtual dict_t fetch() = 0;
};

}}

#endif
