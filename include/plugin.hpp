#ifndef YAPPI_PLUGIN_HPP
#define YAPPI_PLUGIN_HPP

#include <map>
#include <stdexcept>
#include <string>

#include <boost/noncopyable.hpp>

#include "digest.hpp"

#define MAX_SOURCES 10

namespace yappi { namespace plugin {

class exhausted: public std::runtime_error {
    public:
        exhausted(const std::string& message):
            std::runtime_error(message)
        {}
};

// Base class for a plugin source
class source_t: public boost::noncopyable {
    public:
        source_t(const std::string& uri):
            m_uri(uri),
            m_hash(helpers::digest_t().get(m_uri))
        {}

        inline std::string uri() const { return m_uri; }
        inline std::string hash() const { return m_hash; }

        // This method will be called by the scheduler with specified intervals to
        // fetch the new data for publishing
        typedef std::map<std::string, std::string> dict_t;        
        virtual dict_t fetch() = 0;

    protected:
        std::string m_uri, m_hash;
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

typedef const plugin_info_t* (*initialize_fn_t)(void);

}}

#endif
