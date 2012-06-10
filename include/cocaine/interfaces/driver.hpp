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

#ifndef COCAINE_DRIVER_INTERFACE_HPP
#define COCAINE_DRIVER_INTERFACE_HPP

#include <boost/tuple/tuple.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace engine { namespace drivers {

class driver_t:
    public boost::noncopyable
{
    public:
        virtual ~driver_t() {
            // Empty.
        }

        virtual Json::Value info() const = 0;

    public:
        engine_t& engine() {
            return m_engine;
        }

    protected:
        driver_t(context_t& context, engine_t& engine, const plugin_config_t& config):
            m_context(context),
            m_engine(engine)
        { }
        
    private:
        context_t& m_context;
        engine_t& m_engine;
};

}}

template<>
struct category_traits<engine::drivers::driver_t> {
    typedef std::auto_ptr<engine::drivers::driver_t> ptr_type;

    typedef boost::tuple<
        engine::engine_t&,
        const plugin_config_t&
    > args_type;

    template<class T>
    struct default_factory:
        public factory<engine::drivers::driver_t>
    {
        virtual ptr_type get(context_t& context,
                             const args_type& args)
        {
            return ptr_type(
                new T(
                    context,
                    boost::get<0>(args),
                    boost::get<1>(args)
                )
            );
        }
    };
};

}

#endif
