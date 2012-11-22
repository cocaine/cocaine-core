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

#ifndef COCAINE_SYSLOG_SINK_HPP
#define COCAINE_SYSLOG_SINK_HPP

#include "cocaine/api/sink.hpp"

namespace cocaine { namespace sink {

class syslog_t:
    public api::sink_t
{
    public:
        typedef api::sink_t category_type;

    public:
        syslog_t(const std::string& name,
        		 const Json::Value& args);

        virtual
        void
        emit(logging::priorities priority,
             const std::string& source,
             const std::string& message);

    private:
        const std::string m_identity;
};

}} // namespace cocaine::sink

#endif