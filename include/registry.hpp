#ifndef YAPPI_REGISTRY_HPP
#define YAPPI_REGISTRY_HPP

#include <boost/filesystem.hpp>

#include "common.hpp"
#include "plugin.hpp"

namespace yappi { namespace core {

class registry_t:
    public boost::noncopyable,
    public helpers::factory_t<registry_t>
{
    public:
        registry_t(const config_t& config);
        ~registry_t();

        boost::shared_ptr<plugin::source_t> create(const std::string& uri);

    private:
        // Used to instantiate plugin instances
        typedef std::map<const std::string, plugin::factory_fn_t> factory_map_t;
        factory_map_t m_factories;

        // Used to dlclose() all the plugins on shutdown
        std::vector<void*> m_plugins;
};

}}

#endif
