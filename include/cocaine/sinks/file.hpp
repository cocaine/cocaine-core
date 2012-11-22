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

#ifndef COCAINE_FILE_SINK_HPP
#define COCAINE_FILE_SINK_HPP

#include "cocaine/api/sink.hpp"

namespace cocaine { namespace sink {

class file_t:
    public api::sink_t
{
    public:
        typedef api::sink_t category_type;
        
    public:
        file_t(const std::string& name,
               const Json::Value& args);

        virtual
        ~file_t();

        virtual
        void
        emit(logging::priorities,
             const std::string& source,
             const std::string& message);

    private:
        FILE * m_file;
};

}} // namespace cocaine::sink

#endif
