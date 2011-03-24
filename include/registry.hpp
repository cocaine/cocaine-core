#ifndef YAPPI_REGISTRY_HPP
#define YAPPI_REGISTRY_HPP

#include <string>
#include <map>

#include "plugin.hpp"

class registry_t {
    public:
        registry_t(const std::string& directory);
        source_t* create(const std::string& scheme, const std::string& uri);

    private:
        typedef std::map<std::string, factory_t> factories_t;
        factories_t m_factories;
};

#endif
