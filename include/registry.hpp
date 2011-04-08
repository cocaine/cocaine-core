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
        registry_t();
        ~registry_t();

        plugin::source_t* instantiate(const std::string& uri);

    private:
        // Used to instantiate plugin instances
        typedef std::map<std::string, plugin::factory_fn_t> factory_map_t;
        factory_map_t m_factories;

        // Used to dlclose() all the plugins on shutdown
        std::vector<void*> m_plugins;
};

}}

#endif
