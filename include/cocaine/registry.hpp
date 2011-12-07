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

#ifndef COCAINE_REGISTRY_HPP
#define COCAINE_REGISTRY_HPP

#include <ltdl.h>

#include "cocaine/common.hpp"
#include "cocaine/plugin.hpp"

namespace cocaine { namespace core {

class registry_t:
    public boost::noncopyable
{
    public:
        static const boost::shared_ptr<registry_t>& instance();

    public:
        registry_t();
        ~registry_t();

        bool exists(const std::string& type);

        boost::shared_ptr<plugin::source_t> create(const std::string& type,
                                                   const std::string& args);

    private:
        static boost::shared_ptr<registry_t> g_object;
    
    private:
        // Used to instantiate plugin instances
        typedef std::map<const std::string, plugin::factory_fn_t> factory_map_t;
        factory_map_t m_factories;

        // Used to unload all the plugins on shutdown
        std::vector<lt_dlhandle> m_plugins;
};

}}

#endif
