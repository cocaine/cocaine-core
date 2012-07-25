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
#include "cocaine/manifest.hpp"

#include "cocaine/interfaces/driver.hpp"

#include "helpers/json.hpp"

namespace cocaine {

class app_t {
    public:
        app_t(context_t& context,
              const std::string& name);
        
        ~app_t();

        void start();
        void stop();

        Json::Value info() const;
        
        // Job scheduling.
        bool enqueue(const boost::shared_ptr<engine::job_t>& job);

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;
        
        const manifest_t m_manifest;
        std::auto_ptr<engine::engine_t> m_engine;

#if BOOST_VERSION >= 104000
        typedef boost::ptr_unordered_map<
#else
        typedef boost::ptr_map<
#endif
            const std::string,
            engine::drivers::driver_t
        > driver_map_t;
        
        driver_map_t m_drivers;
};

}

#endif
