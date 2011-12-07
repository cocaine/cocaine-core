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

#ifndef COCAINE_DRIVER_BASE_HPP
#define COCAINE_DRIVER_BASE_HPP

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine { namespace driver {

using namespace boost::accumulators;

enum audit_type {
    in_queue,
    on_slave
};

class driver_t:
    public boost::noncopyable
{
    public:
        driver_t(engine_t* engine, const std::string& method);
        virtual ~driver_t();

        void audit(audit_type type, ev::tstamp value);
       
    public:
        virtual Json::Value info() const = 0;

    public: 
        inline engine_t* engine() { 
            return m_engine; 
        }

        inline std::string method() const { 
            return m_method; 
        }
        
    protected:
        Json::Value stats() const;

    protected:
        engine_t* m_engine;
        const std::string m_method;

    private:
        accumulator_set< float, features<tag::sum, tag::median> > m_spent_in_queues;
        accumulator_set< float, features<tag::sum, tag::median> > m_spent_on_slaves;
};

}}}

#endif
