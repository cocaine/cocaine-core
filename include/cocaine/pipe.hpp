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

#ifndef COCAINE_PIPE_HPP
#define COCAINE_PIPE_HPP

#include <boost/weak_ptr.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace engine {

struct pipe_t {
    pipe_t(const boost::shared_ptr<session_t>& session);

    void
    push(const std::string& chunk);

    void
    push(const char * chunk,
         size_t size);

    void
    close();

private:
    boost::weak_ptr<session_t> ptr;
};

}} // namespace cocaine::engine

#endif
