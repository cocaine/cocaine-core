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

#include "cocaine/repository.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/convenience.hpp>

#include <boost/iterator/filter_iterator.hpp>

using namespace cocaine::api;

namespace fs = boost::filesystem;

repository_t::repository_t() {
    if(lt_dlinit() != 0) {
        throw repository_error_t("unable to initialize the dynamic loader");
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
    m_categories.clear();

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
            return fs::is_regular_file(entry) &&
                   entry.path().extension() == ".cocaine-plugin";
        }
    };
}

void
repository_t::load(const std::string& path_) {
    fs::path path(path_);
    validate_t validate;

    auto status = fs::status(path);

    if(!fs::exists(status)) {
        return;
    }

    if(fs::is_directory(status)) {
        typedef boost::filter_iterator<
            validate_t,
            fs::directory_iterator
        > plugin_iterator_t;

        plugin_iterator_t it = plugin_iterator_t(validate, fs::directory_iterator(path)),
                          end;

        while(it != end) {
            // Try to load the plugin.
            open(it->path().string());
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

    lt_dlhandle plugin = lt_dlopenadvise(target.c_str(), advice);
    lt_dladvise_destroy(&advice);

    if(!plugin) {
        throw repository_error_t("unable to load '%s'", target);
    }

    // According to the standard, it is neither defined nor undefined to access
    // a non-active member of a union. But GCC explicitly defines this to be
    // okay, so we do it to avoid warnings about type-punned pointer aliasing.

    union {
        void * ptr;
        initialize_fn_t call;
    } initialize;

    initialize.ptr = lt_dlsym(plugin, "initialize");

    if(initialize.ptr) {
        try {
            initialize.call(*this);
            m_plugins.emplace_back(plugin);
        } catch(const std::exception& e) {
            lt_dlclose(plugin);
            throw repository_error_t("unable to initialize '%s' - %s", target, e.what());
        } catch(...) {
            lt_dlclose(plugin);
            throw repository_error_t("unable to initialize '%s' - unexpected exception", target);
        }
    } else {
        throw repository_error_t("unable to initialize '%s' - initialize() is missing", target);
    }
}

