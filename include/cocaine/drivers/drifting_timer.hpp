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

#ifndef COCAINE_DRIFTING_TIMER_DRIVER_HPP
#define COCAINE_DRIFTING_TIMER_DRIVER_HPP

#include "cocaine/drivers/recurring_timer.hpp"

#include "cocaine/job.hpp"

namespace cocaine { namespace engine { namespace drivers {

class drifting_timer_t;

class drifting_timer_job_t:
    public job_t
{
    public:
        drifting_timer_job_t(drifting_timer_t& driver);
        virtual ~drifting_timer_job_t();
};

class drifting_timer_t:
    public recurring_timer_t
{
    public:
        drifting_timer_t(engine_t& engine,
                         const std::string& method, 
                         const Json::Value& args);

        // Driver interface.
        virtual Json::Value info() /* const */;

        void rearm();

    private:
        // Timer interface.
        virtual void reschedule();
};

}}}

#endif
