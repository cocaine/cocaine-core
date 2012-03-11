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

#ifndef COCAINE_GENERIC_SLAVE_BACKEND_PLUGIN_INTERFACE_HPP
#define COCAINE_GENERIC_SLAVE_BACKEND_PLUGIN_INTERFACE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

#include "cocaine/helpers/data_container.hpp"

namespace cocaine { namespace engine {

class overseer_t;

// Plugin I/O
// ----------

using helpers::data_container_t;

class io_t:
    public boost::noncopyable
{
    public:
        io_t(overseer_t& overseer);

        // Pulls in the next request chunk from the engine.
        data_container_t pull(bool block);

        // Pushes a response chunk to the engine.
        void push(const void * data, size_t size);

        // Pushes a response chunk to be published via the driver's emitter.
        void emit(const std::string& key, const void * data, size_t size);

    private:
        overseer_t& m_overseer;
};

// Generic slave backend plugin interface
// --------------------------------------

class plugin_t:
    public object_t
{
    public:
        plugin_t(context_t& ctx);
        virtual ~plugin_t();

        virtual void initialize(const app_t& app) = 0;
        virtual void invoke(io_t& io, const std::string& method) = 0;
};

}}

#endif
