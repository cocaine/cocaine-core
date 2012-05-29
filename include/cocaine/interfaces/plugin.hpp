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

#ifndef COCAINE_APP_PLUGIN_INTERFACE_HPP
#define COCAINE_APP_PLUGIN_INTERFACE_HPP

#include "cocaine/common.hpp"
#include "cocaine/registry.hpp"

#include "cocaine/helpers/blob.hpp"

namespace cocaine { namespace engine {

class overseer_t;

// App plugin I/O
// --------------

class io_t {
    public:
        io_t(overseer_t& overseer);

        // Pulls in the next request chunk from the engine.
        blob_t pull(int timeout);

        // Pushes a response chunk to the engine.
        void push(const void * data, size_t size);

    private:
        overseer_t& m_overseer;
};

// App plugin interface
// --------------------

class plugin_t {
    public:
        typedef core::policies::none policy;

    public:
        virtual ~plugin_t() = 0;

        virtual void initialize(const app_t& app) = 0;
        virtual void invoke(const std::string& method, io_t& io) = 0;

    protected:
        plugin_t(context_t& context);

    protected:
        context_t& m_context;
};

}}

#endif
