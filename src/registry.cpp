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
    if(lt_dlinit() != 0) {
        throw std::runtime_error("unable to initialize the module loader");
    }

    fs::path path(config_t::get().registry.location);

    if(!fs::exists(path)) {
        throw std::runtime_error(path.string() + " does not exist");
    } else if(fs::exists(path) && !fs::is_directory(path)) {
        throw std::runtime_error(path.string() + " is not a directory");
    }

    lt_dladvise advise;
    
    lt_dladvise_init(&advise);
    lt_dladvise_global(&advise);

    lt_dlhandle plugin;
    initialize_fn_t initializer;
    std::vector<std::string> types;

    // Directory iterator
    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    file_iterator it = file_iterator(is_regular_file(), fs::directory_iterator(path)), end;

    while(it != end) {
        // Load the plugin
#if BOOST_FILESYSTEM_VERSION == 3
        plugin = lt_dlopenadvise(it->path().string().c_str(), advise);
#else
        plugin = lt_dlopenadvise(it->string().c_str(), advise);
#endif

        if(plugin) {
            // Get the plugin info
            initializer = reinterpret_cast<initialize_fn_t>(lt_dlsym(plugin, "initialize"));

            if(initializer) {
                const source_info_t* info = initializer();
                m_plugins.push_back(plugin);

                // Fetch all the available sources from it
                while(info->type && info->factory) {
                    m_factories.insert(std::make_pair(
                        info->type,
                        info->factory));
                    types.push_back(info->type);
                    info++;
                }
            } else {
#if BOOST_FILESYSTEM_VERSION == 3
                syslog(LOG_ERR, "registry: invalid interface in '%s' - %s",
                    it->path().string().c_str(), lt_dlerror());
#else
                syslog(LOG_ERR, "registry: invalid interface in '%s' - %s",
                    it->string().c_str(), lt_dlerror());
#endif
                lt_dlclose(plugin);
            }
        } else {
#if BOOST_FILESYSTEM_VERSION == 3
            syslog(LOG_ERR, "registry: unable to load '%s' - %s", 
                it->path().string().c_str(), lt_dlerror());
#else
            syslog(LOG_ERR, "registry: unable to load '%s' - %s",
                it->string().c_str(), lt_dlerror());
#endif
        }

        ++it;
    }

    if(!m_factories.size()) {
        throw std::runtime_error("no plugins found");
    }

    std::string plugins(boost::algorithm::join(types, ", "));
    syslog(LOG_NOTICE, "registry: available sources - %s", plugins.c_str());
}

registry_t::~registry_t() {
    for(std::vector<lt_dlhandle>::iterator it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        lt_dlclose(*it);
    }

    lt_dlexit();
}

bool registry_t::exists(const std::string& type) {
    return (m_factories.find(type) != m_factories.end());
}

boost::shared_ptr<source_t> registry_t::create(const std::string& type,
                                               const std::string& args) 
{
    factory_map_t::iterator it(m_factories.find(type));
    factory_fn_t factory = it->second;
    
    return boost::shared_ptr<source_t>(factory(args));
}

const boost::shared_ptr<registry_t>& registry_t::instance() {
    if(!g_object.get()) {
        g_object.reset(new registry_t());
    }

    return g_object;
}

boost::shared_ptr<registry_t> registry_t::g_object;
