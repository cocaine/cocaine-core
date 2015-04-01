/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include <blackhole/scoped_attributes.hpp>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/iterator/filter_iterator.hpp>

using namespace cocaine;
using namespace cocaine::api;

namespace fs = boost::filesystem;

namespace {

typedef std::remove_pointer<lt_dlhandle>::type handle_type;

struct lt_dlclose_action_t {
    void
    operator()(handle_type* plugin) const {
        lt_dlclose(plugin);
    }
};

struct validate_t {
    template<typename T>
    bool
    operator()(const T& entry) const {
        return fs::is_regular_file(entry) && entry.path().extension() == ".cocaine-plugin";
    }
};

// Plugin preconditions validation function type.
typedef preconditions_t (*validation_fn_t)();

// Plugin initialization function type.
typedef void (*initialize_fn_t)(repository_t&);

} // namespace

repository_t::repository_t(logging::logger_t& logger):
    m_log(new logging::log_t(logger, blackhole::attribute::set_t({
        logging::keyword::source() = "repository"
    })))
{
    if(lt_dlinit() != 0) throw std::system_error(error::ltdl_error);
}

repository_t::~repository_t() {
    // Destroy all the factories.
    m_categories.clear();

    // Dispose of the plugins.
    std::for_each(m_plugins.begin(), m_plugins.end(), lt_dlclose_action_t());

    // Terminate the dynamic loader.
    lt_dlexit();
}

void
repository_t::load(const std::string& path) {
    const auto status = fs::status(path);

    if(!fs::exists(status) || !fs::is_directory(status)) {
        COCAINE_LOG_ERROR(m_log, "unable to load plugins: path '%s' is not valid", path);
        return;
    }

    typedef boost::filter_iterator<validate_t, fs::directory_iterator> filter_t;

    for(filter_t it = filter_t(validate_t(), fs::directory_iterator(path)), end; it != end; ++it) {
        // Try to load the plugin.
        open(it->path().string());
    }
}

void
repository_t::open(const std::string& target) {
    lt_dladvise advice;
    lt_dladvise_init(&advice);
    lt_dladvise_global(&advice);

    using namespace blackhole;

    scoped_attributes_t attributes(*m_log, { attribute::make("plugin", target) });

    COCAINE_LOG_INFO(m_log, "loading plugin");

    std::unique_ptr<handle_type, lt_dlclose_action_t> plugin(
        lt_dlopenadvise(target.c_str(), advice),
        lt_dlclose_action_t()
    );

    lt_dladvise_destroy(&advice);

    if(!plugin) {
        throw std::system_error(error::ltdl_error, lt_dlerror());
    }

    // According to the standard, it is neither defined nor undefined to access
    // a non-active member of a union. But GCC explicitly defines this to be
    // okay, so we do it to avoid warnings about type-punned pointer aliasing.

    union { void* ptr; validation_fn_t call; } validation;
    union { void* ptr; initialize_fn_t call; } initialize;

    validation.ptr = lt_dlsym(plugin.get(), "validation");
    initialize.ptr = lt_dlsym(plugin.get(), "initialize");

    if(validation.ptr) {
        const auto preconditions = validation.call();

        if(preconditions.version > COCAINE_VERSION) {
            throw std::system_error(error::version_mismatch);
        }
    }

    if(initialize.ptr) {
        try {
            initialize.call(*this);
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to initialize plugin: [%d] %s", e.code().value(),
                e.code().message());
            throw std::system_error(error::initialization_error);
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(m_log, "unable to initialize plugin: %s",
                e.what());
            throw std::system_error(error::initialization_error);
        }
    } else {
        throw std::system_error(error::invalid_interface);
    }

    m_plugins.emplace_back(plugin.release());
}
