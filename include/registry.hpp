#ifndef YAPPI_REGISTRY_HPP
#define YAPPI_REGISTRY_HPP

#include <string>
#include <map>

#include "plugin.hpp"

namespace yappi { namespace core {

class registry_t {
    public:
        registry_t(const std::string& directory);
        plugins::source_t* create(const std::string& scheme, const std::string& uri);

    private:
        typedef std::map<std::string, plugins::factory_t> factories_t;
        factories_t m_factories;
};

}}

#endif
