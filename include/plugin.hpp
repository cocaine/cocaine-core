#ifndef YAPPI_PLUGIN_HPP
#define YAPPI_PLUGIN_HPP

#include "common.hpp"
#include "storage.hpp"

#define MAX_SOURCES 10

namespace yappi { namespace plugin {

class exhausted:
    public std::runtime_error
{
    public:
        exhausted(const std::string& message):
            std::runtime_error(message)
        {}
};

typedef std::map<std::string, std::string> dict_t;        

// Base class for a plugin source
class source_t:
    public boost::noncopyable
{
    public:
        source_t(const std::string& uri):
            m_uri(uri)
        {}

        inline std::string uri() const { return m_uri; }

        enum capabilities_t {
            NONE        = 0,
            ITERATOR    = 1 << 0,
            SCHEDULER   = 1 << 1,
            PROCESSOR   = 1 << 2
        };

        virtual uint32_t capabilities() const = 0;

        // Will be called by the auto timed driver and by the fs driver
        virtual dict_t invoke() {
            throw std::runtime_error("not implemented");
        }

        // Will be called by the manual timed driver
        virtual float reschedule() {
            throw std::runtime_error("not implemented");
        }

        // Will be called by the event driver
        virtual dict_t process(const void* data, size_t data_size) {
            throw std::runtime_error("not implemented");
        }
    
    protected:
        std::string m_uri;
};

// Plugins are expected to supply at least one factory function
// to initialize sources, given an uri string. Each factory function
// is responsible to initialize sources of one registered scheme
typedef source_t* (*factory_fn_t)(const char*);

// Plugins are expected to have an 'initialize' function, which should
// return a pointer to a structure of the following format
struct plugin_info_t {
    unsigned int count;
    struct {
        const char* scheme;
        factory_fn_t factory;
    } sources[MAX_SOURCES];
};

extern "C" {
    typedef const plugin_info_t* (*initialize_fn_t)(void);
}

}}

#endif
