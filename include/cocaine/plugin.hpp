#ifndef COCAINE_PLUGIN_HPP
#define COCAINE_PLUGIN_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace plugin {

class source_t:
    public boost::noncopyable
{
    public:
        virtual Json::Value invoke(
            const std::string& method,
            const void* request = NULL,
            size_t request_size = 0) = 0;
};

// Plugins are expected to supply at least one factory function
// to initialize sources, given an argument. Each factory function
// is responsible to initialize sources of one registered type
typedef source_t* (*factory_fn_t)(const std::string&);

// Plugins are expected to have an 'initialize' function, which should
// return an array of structures of the following format
typedef struct {
    const char* type;
    factory_fn_t factory;
} source_info_t;

extern "C" {
    typedef const source_info_t* (*initialize_fn_t)(void);
}

}}

#endif
