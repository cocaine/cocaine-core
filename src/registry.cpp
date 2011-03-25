#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "common.hpp"
#include "registry.hpp"

using namespace yappi::core;
using namespace yappi::plugins;

namespace {
    int filter(const dirent *entry) {
        return (entry->d_type == DT_REG && strstr(entry->d_name, ".so"));
    }
}

registry_t::registry_t(const std::string& directory) {
    // Scan for all files in the specified directory
    dirent **namelist;
    int count = scandir(
        directory.c_str(),
        &namelist,
        &filter,
        alphasort);

    if(count == -1) {
        std::string message = directory + ": " + strerror(errno);
        throw std::runtime_error(message);
    }

    void *plugin;
    std::string path; 
    
    typedef const plugin_info_t* (*initialize_fn_t)(void);
    initialize_fn_t initialize;

    while(count--) {
        // Load the plugin
        path = directory + '/' + namelist[count]->d_name;
        plugin = dlopen(path.c_str(), RTLD_LAZY);
        
        if(!plugin) {
            syslog(LOG_ERR, "failed to load %s", dlerror());
            continue;
        }

        // Get the plugin info
        initialize = reinterpret_cast<initialize_fn_t>(dlsym(plugin, "initialize"));

        if(!initialize) {
            syslog(LOG_ERR, "invalid plugin interface: %s", dlerror());
            continue;
        }

        const plugin_info_t* info = initialize();

        // Fetch all supported sources from it
        for(unsigned int i = 0; i < info->count; ++i) {
            m_factories[info->source[i].scheme] = info->source[i].factory;
        }

        // Free the directory entry
        free(namelist[count]);
    }
    
    if(!m_factories.size()) {
        throw std::runtime_error("no plugins found, terminating");
    }

    std::ostringstream fmt;
    for(factories_t::iterator it = m_factories.begin(); it != m_factories.end(); ++it) {
        fmt << " " << it->first;
    }

    syslog(LOG_INFO, "available sources:%s", fmt.str().c_str());
}

source_t* registry_t::create(const std::string& scheme, const std::string& uri) {
    factories_t::iterator it = m_factories.find(scheme);

    if(it == m_factories.end()) {
        throw std::domain_error(scheme);
    }

    factory_t factory = it->second;
    return reinterpret_cast<source_t*>(factory(uri.c_str()));
}
