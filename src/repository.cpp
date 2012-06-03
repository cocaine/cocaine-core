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

struct is_module {
    template<typename T> 
    bool operator()(const T& entry) {
        return fs::is_regular(entry) &&
               entry.path().extension() == ".cocaine-module";
    }
};

repository_t::repository_t(context_t& context):
    m_context(context),
    m_log(context.log("registry"))
{
    if(lt_dlinit() != 0) {
        throw registry_error_t("unable to initialize the module loader");
    }

    fs::path path(m_context.config.module_path);

    if(!fs::exists(path)) {
        throw configuration_error_t("module path '" + path.string() + "' does not exist");
    } else if(fs::exists(path) && !fs::is_directory(path)) {
        throw configuration_error_t("module path '" + path.string() + "' is not a directory");
    }

    lt_dladvise advice;
    lt_dladvise_init(&advice);
    lt_dladvise_global(&advice);

    lt_dlhandle module;

    typedef void (*initialize_fn_t)(repository_t& registry);
    initialize_fn_t initialize = NULL;

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
    struct dispose {
        template<class T>
        void operator()(T& module) {
            lt_dlclose(module);
        }
    };
}

repository_t::~repository_t() {
    std::for_each(m_modules.begin(), m_modules.end(), dispose());
    lt_dlexit();
}

