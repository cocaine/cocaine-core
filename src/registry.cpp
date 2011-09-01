#include <dlfcn.h>

#include <boost/iterator/filter_iterator.hpp>
#include <boost/algorithm/string/join.hpp>

#include "common.hpp"
#include "registry.hpp"

using namespace yappi::core;
using namespace yappi::plugin;
namespace fs = boost::filesystem;

struct is_regular_file {
    template<typename T> bool operator()(T entry) {
        return fs::is_regular(entry);
    }
};

registry_t::registry_t(const config_t& config) {
    fs::path path = config.paths.plugins;

    if(fs::exists(path) && !fs::is_directory(path)) {
        throw std::runtime_error(path.string() + " is not a directory");
    }

    if(!fs::exists(path)) {
        try {
            fs::create_directories(path);
        } catch(const std::runtime_error& e) {
            throw std::runtime_error("cannot create " + path.string());
        }
    }

    void* plugin;
    initialize_fn_t initializer;
    std::vector<std::string> schemes;

    // Directory iterator
    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    file_iterator it = file_iterator(is_regular_file(),
        fs::directory_iterator(path)), end;

    while(it != end) {
        // Load the plugin
        plugin = dlopen(it->string().c_str(), RTLD_NOW | RTLD_GLOBAL);
        
        if(plugin) {
            // Get the plugin info
            initializer = reinterpret_cast<initialize_fn_t>(dlsym(plugin, "initialize"));

            if(initializer) {
                const plugin_info_t* info = initializer();
                m_plugins.push_back(plugin);

                // Fetch all the available sources from it
                for(unsigned int i = 0; i < info->count; ++i) {
                    m_factories.insert(std::make_pair(
                        info->sources[i].scheme,
                        info->sources[i].factory));
                    schemes.push_back(info->sources[i].scheme);
                }
            } else {
                syslog(LOG_ERR, "registry: invalid plugin interface - %s", dlerror());
                dlclose(plugin);
            }
        } else {
            syslog(LOG_ERR, "registry: failed to load %s", dlerror());
        }

        ++it;
    }

    if(!m_factories.size()) {
        throw std::runtime_error("no plugins found");
    }

    std::string plugins = boost::algorithm::join(schemes, ", ");
    syslog(LOG_NOTICE, "registry: available sources - %s", plugins.c_str());
}

registry_t::~registry_t() {
    for(std::vector<void*>::iterator it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        dlclose(*it);
    }
}

boost::shared_ptr<source_t> registry_t::instantiate(const std::string& uri) {
    std::string scheme = uri.substr(0, uri.find_first_of(":"));
    factory_map_t::iterator it = m_factories.find(scheme);

    if(it == m_factories.end()) {
        throw std::runtime_error(scheme + " plugin not found");
    }

    factory_fn_t factory = it->second;
    return boost::shared_ptr<source_t>(factory(uri.c_str()));
}
