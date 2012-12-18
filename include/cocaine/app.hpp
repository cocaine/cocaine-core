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

#ifndef COCAINE_APP_HPP
#define COCAINE_APP_HPP

#include "cocaine/common.hpp"

#include "cocaine/helpers/json.hpp"

#include <boost/thread/thread.hpp>

namespace cocaine {

class app_t {
    public:
        app_t(context_t& context,
              const std::string& name,
              const std::string& profile);
        
        ~app_t();

        void
        start();
        
        void
        stop();

        Json::Value
        info() const;
        
        // Scheduling
        
        boost::shared_ptr<api::stream_t>
        enqueue(const api::event_t& event,
                const boost::shared_ptr<api::stream_t>& upstream,
                engine::mode mode = engine::mode::normal);

    private:
        void
        deploy(const std::string& name,
               const std::string& path);

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // Configuration

        std::unique_ptr<const manifest_t> m_manifest;
        std::unique_ptr<const profile_t> m_profile;

        // I/O

        std::unique_ptr<io::unique_channel_t> m_control;

        // Execution engine

        std::unique_ptr<engine::engine_t> m_engine;
        std::unique_ptr<boost::thread> m_thread;

        // Event drivers

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string,
            std::unique_ptr<api::driver_t>
        > driver_map_t;
        
        driver_map_t m_drivers;
};

} // namespace cocaine

#endif
