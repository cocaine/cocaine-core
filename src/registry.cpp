#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "common.hpp"
#include "registry.hpp"
#include "uri.hpp"

using namespace yappi::core;
using namespace yappi::plugin;

namespace {
    // TODO: Make this portable
    int filter(const dirent *entry) {
        return (entry->d_type == DT_REG && strstr(entry->d_name, ".so"));
    }
}

registry_t::registry_t() {
    std::string directory = "/usr/lib/yappi";
    dirent** namelist;
    
    // Scan for all files in the specified directory
    int count = scandir(
        directory.c_str(),
        &namelist,
        &filter,
        alphasort);

    if(count == -1) {
        std::string message = directory + ": " + strerror(errno);
        throw std::runtime_error(message);
    }
    
    void* plugin;
    std::string path; 
    initialize_t initializer;

    while(count--) {
        // Load the plugin
        // TODO: Make this portable, too.
        path = directory + '/' + namelist[count]->d_name;
        plugin = dlopen(path.c_str(), RTLD_NOW);
        
        if(!plugin) {
            syslog(LOG_ERR, "failed to load %s", dlerror());
            continue;
        }

        // Get the plugin info
        initializer = reinterpret_cast<initialize_t>(dlsym(plugin, "initialize"));

        if(!initializer) {
            syslog(LOG_ERR, "invalid plugin interface: %s", dlerror());
            dlclose(plugin);
            continue;
        }
        
        m_plugins.push_back(plugin);
        const plugin_info_t* info = initializer();

        // Fetch all the available sources from it
        for(unsigned int i = 0; i < info->count; ++i) {
            m_factories.insert(std::make_pair(
                info->sources[i].scheme,
                info->sources[i].factory));
        }

        free(namelist[count]);
    }

    free(namelist);
    
    if(!m_factories.size()) {
        throw std::runtime_error("no plugins found");
    }

    std::ostringstream fmt;
    for(factory_map_t::iterator it = m_factories.begin(); it != m_factories.end(); ++it) {
        fmt << " " << it->first;
    }

    syslog(LOG_INFO, "available sources:%s", fmt.str().c_str());
}

registry_t::~registry_t() {
    for(std::vector<void*>::iterator it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        dlclose(*it);
    }
}

source_t* registry_t::instantiate(const std::string& uri_) {
    helpers::uri_t uri(uri_);    
    factory_map_t::iterator it = m_factories.find(uri.scheme());

    if(it == m_factories.end()) {
        throw std::domain_error(uri.scheme());
    }

    factory_fn_t factory = it->second;
    return reinterpret_cast<source_t*>(factory(uri.source().c_str()));
}
