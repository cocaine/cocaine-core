//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <boost/filesystem.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/algorithm/string/join.hpp>

#include "cocaine/context.hpp"
#include "cocaine/registry.hpp"

using namespace cocaine;
using namespace cocaine::core;
using namespace cocaine::plugin;

namespace fs = boost::filesystem;

struct is_regular_file {
    template<typename T> bool operator()(T entry) {
        return fs::is_regular(entry);
    }
};

registry_t::registry_t(context_t& context) {
    if(lt_dlinit() != 0) {
        throw std::runtime_error("unable to initialize the module loader");
    }

    fs::path path(context.config.registry.location);

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
        std::string plugin_path = it->path().string();
#else
        std::string plugin_path = it->string();
#endif

        plugin = lt_dlopenadvise(plugin_path.c_str(), advise);

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

const boost::shared_ptr<registry_t>& registry_t::instance(context_t& context) {
    if(!g_object.get()) {
        g_object.reset(new registry_t(context));
    }

    return g_object;
}

boost::shared_ptr<registry_t> registry_t::g_object;
