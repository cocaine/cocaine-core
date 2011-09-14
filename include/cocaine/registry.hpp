#ifndef COCAINE_REGISTRY_HPP
#define COCAINE_REGISTRY_HPP

#include "cocaine/common.hpp"
#include "cocaine/plugin.hpp"

namespace cocaine { namespace core {

class registry_t:
    public boost::noncopyable
{
    public:
        static boost::shared_ptr<registry_t> instance();

    public:
        registry_t();
        ~registry_t();

        std::auto_ptr<plugin::source_t> create(const std::string& uri);

    private:
        // Used to instantiate plugin instances
        typedef std::map<const std::string, plugin::factory_fn_t> factory_map_t;
        factory_map_t m_factories;

        // Used to dlclose() all the plugins on shutdown
        std::vector<void*> m_plugins;

    private:
        static boost::shared_ptr<registry_t> g_object;
};

}}

#endif
