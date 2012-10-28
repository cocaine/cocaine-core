/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#ifndef COCAINE_MASTER_HPP
#define COCAINE_MASTER_HPP

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/events.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/api/isolate.hpp"

namespace cocaine { namespace engine {

class master_t:
    public boost::noncopyable
{
    public:
        master_t(context_t& context,
                 ev::loop_ref& loop,
                 const manifest_t& manifest,
                 const profile_t& profile);

        ~master_t();
       
        const unique_id_t&
        id() const;

        bool
        operator==(const master_t& other) const;

        void
        process(const events::heartbeat& event);
        
        void
        process(const events::terminate& event);
 
        void
        process(const events::invoke& event);
 
        void
        process(const events::chunk& event);
 
        void
        process(const events::error& event);
 
       void
        process(const events::choke& event);

    private:
        void
        on_timeout(ev::timer&, int);
 
    public:
        struct state {
            enum value: int {
                unknown,
                idle,
                busy,
                dead
            };
        };

        // The current slave state.
        state::value state;

        // The current job.
        boost::shared_ptr<job_t> job;

    private:
        context_t& m_context; 
        boost::shared_ptr<logging::logger_t> m_log;

        ev::loop_ref& m_loop;
        ev::timer m_heartbeat_timer;
    
        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Host-unique identifier for this slave.
        const unique_id_t m_id;

        // The actual slave handle.    
        std::unique_ptr<api::handle_t> m_handle;
};

}} // namespace cocaine::engine

#endif
