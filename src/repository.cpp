//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/repository.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

namespace fs = boost::filesystem;

struct is_plugin {
    template<typename T> 
    bool operator()(const T& entry) {
        return fs::is_regular(entry) &&
               entry.path().extension() == ".cocaine-plugin";
    }
};

repository_t::repository_t(context_t& context):
    m_context(context),
    m_log(context.log("repository"))
{
    if(lt_dlinit() != 0) {
        throw registry_error_t("unable to initialize the plugin loader");
    }

    fs::path path(m_context.config.plugin_path);

    if(!fs::exists(path)) {
        throw configuration_error_t("plugin path '" + path.string() + "' does not exist");
    } else if(fs::exists(path) && !fs::is_directory(path)) {
        throw configuration_error_t("plugin path '" + path.string() + "' is not a directory");
    }

    lt_dladvise advice;
    lt_dladvise_init(&advice);
    lt_dladvise_global(&advice);

    lt_dlhandle plugin;

    typedef void (*initialize_fn_t)(repository_t& registry);
    initialize_fn_t initialize = NULL;

    typedef boost::filter_iterator<is_plugin, fs::directory_iterator> plugin_iterator_t;
    plugin_iterator_t it = plugin_iterator_t(is_plugin(), fs::directory_iterator(path)), 
                      end;

    while(it != end) {
        // Try to load the plugin.
#if BOOST_FILESYSTEM_VERSION == 3
        std::string plugin_path = it->path().string();
#else
        std::string plugin_path = it->string();
#endif

        plugin = lt_dlopenadvise(plugin_path.c_str(), advice);

        if(plugin) {
            // Try to get the initialization routine.
            initialize = reinterpret_cast<initialize_fn_t>(lt_dlsym(plugin, "initialize"));

            if(initialize) {
                initialize(*this);
                m_plugins.push_back(plugin);
            } else {
                m_log->error(
                    "'%s' is not a cocaine plugin",
#if BOOST_FILESYSTEM_VERSION == 3
                    it->path().string().c_str()
#else
                    it->string().c_str()
#endif
                );

                lt_dlclose(plugin);
            }
        } else {
            m_log->error(
                "unable to load '%s' - %s",
#if BOOST_FILESYSTEM_VERSION == 3
                it->path().string().c_str(), 
#else
                it->string().c_str(),
#endif
                lt_dlerror()
            );
        }

        ++it;
    }

    lt_dladvise_destroy(&advice);
}

namespace {
    struct disposer {
        template<class T>
        void operator()(T& plugin) {
            lt_dlclose(plugin);
        }
    };
}

repository_t::~repository_t() {
    std::for_each(m_plugins.begin(), m_plugins.end(), disposer());
    lt_dlexit();
}

