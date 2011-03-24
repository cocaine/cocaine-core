#ifndef YAPPI_PLUGIN_HPP
#define YAPPI_PLUGIN_HPP

#include <string>
#include <map>

#define MAX_SOURCES_PER_PLUGIN 10

typedef void*(*factory_t)(const char*);

extern "C" {
    struct plugin_info_t {
        unsigned int count;
    
        struct source_info_t {
            const char* scheme;
            factory_t factory;
        } source[MAX_SOURCES_PER_PLUGIN];
    };
}

typedef std::map<std::string, std::string> dict_t;        

class source_t {
    public:
        virtual dict_t fetch() = 0;
};

#endif
