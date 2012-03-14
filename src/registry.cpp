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

#include "cocaine/registry.hpp"

#include "cocaine/context.hpp"

using namespace cocaine::core;

namespace fs = boost::filesystem;

struct is_module {
    template<typename T> 
    bool operator()(const T& entry) {
        return fs::is_regular(entry) 
               && entry.path().extension() == ".cocaine-module";
    }
};

registry_t::registry_t(context_t& ctx):
    object_t(ctx),
    m_log(ctx.log("registry"))
{
    if(lt_dlinit() != 0) {
        throw std::runtime_error("unable to initialize the module loader");
    }

    fs::path path(context().config.core.modules);

    if(!fs::exists(path)) {
        throw std::runtime_error("path '" + path.string() + "' does not exist");
    } else if(fs::exists(path) && !fs::is_directory(path)) {
        throw std::runtime_error("path '" + path.string() + "' is not a directory");
    }

    lt_dladvise advice;
    lt_dladvise_init(&advice);
    lt_dladvise_global(&advice);

    lt_dlhandle module;

    typedef void (*initialize_fn_t)(registry_t& registry);
    initialize_fn_t initialize;

    typedef boost::filter_iterator<is_module, fs::directory_iterator> module_iterator_t;
    module_iterator_t it = module_iterator_t(is_module(), fs::directory_iterator(path)), 
                      end;

    while(it != end) {
        // Try to load the module.
#if BOOST_FILESYSTEM_VERSION == 3
        std::string module_path = it->path().string();
#else
        std::string module_path = it->string();
#endif

        module = lt_dlopenadvise(module_path.c_str(), advice);

        if(module) {
            // Try to get the initialization routine.
            initialize = reinterpret_cast<initialize_fn_t>(lt_dlsym(module, "initialize"));

            if(initialize) {
                initialize(*this);
                m_modules.push_back(module);
            } else {
                m_log->error(
                    "'%s' is not a cocaine module",
#if BOOST_FILESYSTEM_VERSION == 3
                    it->path().string().c_str()
#else
                    it->string().c_str()
#endif
                );

                lt_dlclose(module);
            }

            initialize = NULL;
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

        module = NULL;
        ++it;
    }
}

namespace {
    struct dispose {
        template<class T>
        void operator()(T& module) {
            lt_dlclose(module);
        }
    };
}

registry_t::~registry_t() {
    std::for_each(m_modules.begin(), m_modules.end(), dispose());
    lt_dlexit();
}
