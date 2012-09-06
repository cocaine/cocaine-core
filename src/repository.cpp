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

using namespace cocaine::api;

namespace fs = boost::filesystem;

repository_t::repository_t(context_t& context):
    m_context(context),
    m_log(context.log("repository"))
{
    if(lt_dlinit() != 0) {
        throw repository_error_t("unable to initialize the plugin loader");
    }
}

namespace {
    struct dispose_t {
        template<class T>
        void
        operator()(T& plugin) const {
            lt_dlclose(plugin);
        }
    };
}

repository_t::~repository_t() {
    // Destroy all the factories.
    m_factories.clear();

    // Dispose of the plugins.
    std::for_each(m_plugins.begin(), m_plugins.end(), dispose_t());
    
    // Terminate the dynamic loader.
    lt_dlexit();
}

namespace {
    struct validate_t {
        template<typename T> 
        bool
        operator()(const T& entry) const {
            return fs::is_regular(entry) &&
                   entry.path().extension() == ".cocaine-plugin";
        }
    };
}

void
repository_t::load(const std::string& path_) {
    fs::path path(path_);
    validate_t validate;

    if(fs::is_directory(path)) {
        typedef boost::filter_iterator<
            validate_t,
            fs::directory_iterator
        > plugin_iterator_t;
        
        plugin_iterator_t it = plugin_iterator_t(validate, fs::directory_iterator(path)), 
                          end;

        while(it != end) {
            // Try to load the plugin.
#if BOOST_FILESYSTEM_VERSION == 3
            std::string leaf = it->path().string();
#else
            std::string leaf = it->string();
#endif

            open(leaf);

            ++it;
        }
    } else {
        // NOTE: Just try to open the file.
        open(path.string());
    }
}

void
repository_t::open(const std::string& target) {
    lt_dladvise advice;
    lt_dladvise_init(&advice);
    lt_dladvise_global(&advice);

    m_log->debug(
        "loading components from '%s'",
        target.c_str()
    );

    lt_dlhandle plugin = lt_dlopenadvise(
        target.c_str(),
        advice
    );

    lt_dladvise_destroy(&advice);
    
    if(plugin) {
        // Try to get the initialization routine.
        initialize_fn_t initialize = reinterpret_cast<initialize_fn_t>(
            lt_dlsym(plugin, "initialize")
        );

        if(initialize) {
            try {
                initialize(*this);
                m_plugins.push_back(plugin);
                return;
            } catch(const std::exception& e) {
                m_log->error(
                    "unable to initialize '%s' - %s",
                    target.c_str(),
                    e.what()
                );
            } catch(...) {
                m_log->error(
                    "unable to initialize '%s' - unexpected exception",
                    target.c_str()
                );
            }
        } else {
            m_log->error(
                "unable to initialize '%s' - invalid interface",
                target.c_str()
            );
        }

        lt_dlclose(plugin);
    } else {
        m_log->error(
            "unable to load '%s' - %s",
            target.c_str(),
            lt_dlerror()
        );
    }
}

