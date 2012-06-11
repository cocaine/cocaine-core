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

#ifndef COCAINE_ELLIPTICS_STORAGE_HPP
#define COCAINE_ELLIPTICS_STORAGE_HPP

#include <elliptics/cppdef.h>

#include "cocaine/interfaces/storage.hpp"

namespace cocaine { namespace storages {

class log_adapter_t:
    public zbr::elliptics_log
{
    public:
        log_adapter_t(const boost::shared_ptr<logging::logger_t>& log,
                      const uint32_t mask);

        virtual void log(const uint32_t mask, const char * message);
        virtual unsigned long clone();

    private:
        boost::shared_ptr<logging::logger_t> m_log;
        const uint32_t m_mask;
};

class elliptics_storage_t:
    public storage_concept<objects>
{
    public:
        typedef storage_concept<objects> category_type;

    public:
        elliptics_storage_t(context_t& context,
                            const plugin_config_t& config);

        virtual objects::value_type get(const std::string& ns,
                                        const std::string& key);

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const objects::value_type& object);

        virtual objects::meta_type exists(const std::string& ns,
                                          const std::string& key);

        virtual std::vector<std::string> list(const std::string& ns);

        virtual void remove(const std::string& ns,
                            const std::string& key);

    private:
        std::string id(const std::string& ns,
                       const std::string& key)
        {
            return ns + '\0' + key;
        };

    private:
        boost::shared_ptr<logging::logger_t> m_log;
        
        log_adapter_t m_log_adapter;
        zbr::elliptics_node m_node;
};

}}

#endif
