#include <dlfcn.h>

#include <boost/filesystem.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/algorithm/string/join.hpp>

#include "cocaine/registry.hpp"

using namespace cocaine::core;
using namespace cocaine::plugin;

namespace fs = boost::filesystem;

struct is_regular_file {
    template<typename T> bool operator()(T entry) {
        return fs::is_regular(entry);
    }
};

registry_t::registry_t() {
    fs::path path(config_t::get().registry.location);

    if(!fs::exists(path)) {
        throw std::runtime_error(path.string() + " does not exist");
    } else if(fs::exists(path) && !fs::is_directory(path)) {
        throw std::runtime_error(path.string() + " is not a directory");
    }

    void* plugin;
    initialize_fn_t initializer;
    std::vector<std::string> schemes;

    // Directory iterator
    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    file_iterator it = file_iterator(is_regular_file(), fs::directory_iterator(path)), end;

    while(it != end) {
        // Load the plugin
#if BOOST_FILESYSTEM_VERSION == 3
        plugin = dlopen(it->path().string().c_str(), RTLD_NOW | RTLD_GLOBAL);
#else
        plugin = dlopen(it->string().c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif

        if(plugin) {
            // Get the plugin info
            initializer = reinterpret_cast<initialize_fn_t>(dlsym(plugin, "initialize"));

            if(initializer) {
                const source_info_t* info = initializer();
                m_plugins.push_back(plugin);

                // Fetch all the available sources from it
                while(info->scheme && info->factory) {
                    m_factories.insert(std::make_pair(
                        info->scheme,
                        info->factory));
                    schemes.push_back(info->scheme);
                    info++;
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

    std::string plugins(boost::algorithm::join(schemes, ", "));
    syslog(LOG_NOTICE, "registry: available sources - %s", plugins.c_str());
}

registry_t::~registry_t() {
    for(std::vector<void*>::iterator it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        dlclose(*it);
    }
}

std::auto_ptr<source_t> registry_t::create(const std::string& uri) {
    std::string scheme(uri.substr(0, uri.find_first_of(":")));
    factory_map_t::iterator it(m_factories.find(scheme));

    if(it == m_factories.end()) {
        throw std::runtime_error("'" + scheme + "' plugin is not available");
    }

    factory_fn_t factory = it->second;
    return std::auto_ptr<source_t>(factory(uri.c_str()));
}

boost::shared_ptr<registry_t> registry_t::instance() {
    if(!g_object.get()) {
        g_object.reset(new registry_t());
    }

    return g_object;
}

boost::shared_ptr<registry_t> registry_t::g_object;
