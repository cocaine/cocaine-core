#ifndef YAPPI_REGISTRY_HPP
#define YAPPI_REGISTRY_HPP

#include <string>
#include <vector>
#include <map>

#include "plugin.hpp"
#include "uri.hpp"

namespace yappi { namespace core {

using namespace helpers;

class registry_t {
    public:
        registry_t(const std::string& directory);
        ~registry_t();

        plugin::source_t* create(const uri_t& uri);

    private:
        // This is needed to dlclose() all the opened plugins
        std::vector<void*> m_plugins;

        // This is the actual source registry
        typedef std::map<std::string, plugin::factory_t> factory_map_t;
        factory_map_t m_factories;
};

}}

#endif
