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

#ifndef COCAINE_SLAVE_THREAD_HPP
#define COCAINE_SLAVE_THREAD_HPP

#include <boost/thread/thread.hpp>

#include <cocaine/slaves/base.hpp>

namespace cocaine { namespace engine { namespace slave {

class thread_t:
    public slave_t
{
    public:        
        thread_t(engine_t& engine,
                 const std::string& type,
                 const std::string& args);

        virtual void reap();

    private:
        boost::shared_ptr<overseer_t> m_overseer;
        boost::shared_ptr<boost::thread> m_thread;
};

}}}

#endif
