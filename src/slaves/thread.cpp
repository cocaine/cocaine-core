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

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/slaves/thread.hpp"

#if BOOST_VERSION < 103500
# include <boost/bind.hpp>
#endif

using namespace cocaine::engine::slave;

thread_t::thread_t(engine_t& engine, const std::string& type, const std::string& args):
    slave_t(engine)
{
    try {
        m_overseer.reset(new overseer_t(m_engine.context(), id(), m_engine.name()));
#if BOOST_VERSION >= 103500
        m_thread.reset(new boost::thread(boost::ref(*m_overseer), type, args));
#else
        m_thread.reset(new boost::thread(boost::bind(&overseer_t::operator(), m_overseer.get(), type, args)));
#endif
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("clone() failed");
    }
}

void thread_t::reap() {
#if BOOST_VERSION >= 103500
    if(!m_thread->timed_join(boost::posix_time::seconds(5))) {
        m_thread->interrupt();
        m_thread.reset();
    }
#else
    m_thread.reset();
#endif
}

