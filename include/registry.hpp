#ifndef YAPPI_REGISTRY_HPP
#define YAPPI_REGISTRY_HPP

#include <string>
#include <vector>
#include <map>

#include "plugin.hpp"

namespace yappi { namespace core {

class registry_t {
    public:
        registry_t(const std::string& directory);
        ~registry_t();

        plugins::source_t* create(const std::string& scheme, const std::string& uri);

    private:
        typedef std::vector<void*> plugins_t;
        plugins_t m_plugins;

        typedef std::map<std::string, plugins::factory_fn_t> factories_t;
        factories_t m_factories;
};

}}

#endif
