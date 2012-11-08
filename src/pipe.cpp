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

#include "cocaine/pipe.hpp"

#include "cocaine/session.hpp"

using namespace cocaine::engine;

pipe_t::pipe_t(const boost::shared_ptr<session_t>& session):
    ptr(session)
{ }

void
pipe_t::push(const std::string& chunk) {
    boost::shared_ptr<session_t> session(ptr.lock());

    if(!session) {
        throw cocaine::error_t("session does not exists");
    }

    session->push(chunk);
}

void
pipe_t::push(const char * chunk,
             size_t size)
{
    push({chunk, size});
}

void
pipe_t::close() {
    boost::shared_ptr<session_t> session(ptr.lock());

    if(!session) {
        return;
    }

    session->close();
}
