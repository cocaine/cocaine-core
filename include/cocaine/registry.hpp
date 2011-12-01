#ifndef COCAINE_REGISTRY_HPP
#define COCAINE_REGISTRY_HPP

#include <ltdl.h>

#include "cocaine/common.hpp"
#include "cocaine/plugin.hpp"

namespace cocaine { namespace core {

class registry_t:
    public boost::noncopyable
{
    public:
        static const boost::shared_ptr<registry_t>& instance();

    public:
        registry_t();
        ~registry_t();

        bool exists(const std::string& type);

        boost::shared_ptr<plugin::source_t> create(const std::string& type,
                                                   const std::string& args);

    private:
        static boost::shared_ptr<registry_t> g_object;
    
    private:
        // Used to instantiate plugin instances
        typedef std::map<const std::string, plugin::factory_fn_t> factory_map_t;
        factory_map_t m_factories;

        // Used to unload all the plugins on shutdown
        std::vector<lt_dlhandle> m_plugins;
};

}}

#endif
