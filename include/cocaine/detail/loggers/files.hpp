/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_FILE_LOGGER_HPP
#define COCAINE_FILE_LOGGER_HPP

#include "cocaine/api/logger.hpp"

namespace cocaine { namespace logger {

class files_t:
    public api::logger_t
{
    public:
        files_t(const config_t& config, const Json::Value& args);

        virtual
       ~files_t();

        virtual
        void
        emit(logging::priorities level, const std::string& source, const std::string& message);

        void
        reopen();

    private:
        std::string m_path;
        FILE* m_file;
};

}} // namespace cocaine::logger

#include "cocaine/api/service.hpp"

namespace cocaine { namespace service {

class file_logger_t:
    public api::service_t
{
    public:
        file_logger_t(context_t& context, io::reactor_t& reactor, const std::string& name, const Json::Value& args);

    private:
        api::category_traits<api::logger_t>::ptr_type m_underlying;
        logger::files_t* m_logger;
};

}}

#endif
