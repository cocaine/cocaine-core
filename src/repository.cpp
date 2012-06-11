/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <boost/filesystem.hpp>
#include <boost/iterator/filter_iterator.hpp>

#include "cocaine/repository.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

namespace fs = boost::filesystem;

factory_concept_t::~factory_concept_t() { }

struct is_plugin {
    template<typename T> 
    bool operator()(const T& entry) const {
        return fs::is_regular(entry) &&
               entry.path().extension() == ".cocaine-plugin";
    }
};

repository_t::repository_t(context_t& context):
    m_context(context),
    m_log(context.log("repository"))
{
    if(lt_dlinit() != 0) {
        throw repository_error_t("unable to initialize the plugin loader");
    }

    fs::path path(m_context.config.plugin_path);

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
        void operator()(T& plugin) const {
            lt_dlclose(plugin);
        }
    };
}

repository_t::~repository_t() {
    // Destroy all the factories.
    m_categories.clear();

    // Dispose of the plugins.
    std::for_each(m_plugins.begin(), m_plugins.end(), disposer());
    
    // Terminate the dynamic loader.
    lt_dlexit();
}

