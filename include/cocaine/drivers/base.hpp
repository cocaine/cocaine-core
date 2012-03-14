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

#ifndef COCAINE_DRIVER_BASE_HPP
#define COCAINE_DRIVER_BASE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

#include "cocaine/networking.hpp"

#if BOOST_VERSION >= 103600
# include <boost/accumulators/accumulators.hpp>
# include <boost/accumulators/statistics/median.hpp>
# include <boost/accumulators/statistics/sum.hpp>
#endif

namespace cocaine { namespace engine { namespace drivers {

#if BOOST_VERSION >= 103600
using namespace boost::accumulators;
#endif

enum timing_type {
    in_queue,
    on_slave
};

class driver_t:
    public boost::noncopyable,
    public object_t
{
    public:
        virtual ~driver_t();

        // Collects various statistical information about the driver.
        void audit(timing_type type, ev::tstamp value);

        // Retrieves the runtime statistics from the driver.
        virtual Json::Value info() const;

    public:
        inline const engine_t& engine() {
            return m_engine;
        }

        inline const std::string& method() const {
            return m_method;
        }
    
    protected:
       driver_t(engine_t& engine,
                 const std::string& method,
                 const Json::Value& args);
        
    protected:
        engine_t& m_engine;
        const std::string m_method;

    private:
        boost::shared_ptr<networking::socket_t> m_emitter;

#if BOOST_VERSION >= 103600
        accumulator_set< float, features<tag::sum, tag::median> > m_spent_in_queues;
        accumulator_set< float, features<tag::sum, tag::median> > m_spent_on_slaves;
#else
        float m_spent_in_queues;
        float m_spent_on_slaves;
#endif
};

}}}

#endif
