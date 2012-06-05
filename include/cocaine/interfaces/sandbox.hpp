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

#ifndef COCAINE_SANDBOX_INTERFACE_HPP
#define COCAINE_SANDBOX_INTERFACE_HPP

#include <boost/tuple/tuple.hpp>

#include "cocaine/common.hpp"
#include "cocaine/context.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/blob.hpp"

namespace cocaine { namespace engine {

// Sandbox I/O
// -----------

class io_t {
    public:
        virtual blob_t read(int timeout) = 0;

        virtual void write(const void * data,
                           size_t size) = 0;
};

// Sandbox interface
// -----------------

class sandbox_t {
    public:
        virtual ~sandbox_t() {
            // Empty.
        }
        
        virtual void invoke(const std::string& method,
                            io_t& io) = 0;

    public:
        const logging::logger_t& log() const {
            return *m_log;
        }
    
    protected:
        sandbox_t(context_t& context, const manifest_t& manifest):
            m_context(context),
            m_log(m_context.log(manifest.name)),
            m_manifest(manifest)
        { }

    protected:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        const manifest_t& m_manifest;
};

}

template<> struct category_traits<engine::sandbox_t> {
    typedef std::auto_ptr<engine::sandbox_t> ptr_type;
    typedef boost::tuple<const engine::manifest_t&> args_type;

    template<class T>
    struct default_factory:
        public factory<engine::sandbox_t>
    {
        virtual ptr_type get(context_t& context,
                             const args_type& args)
        {
            return ptr_type(new T(context, boost::get<0>(args)));
        }
    };
};

}

#endif
