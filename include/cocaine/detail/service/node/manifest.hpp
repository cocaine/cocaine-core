/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_ENGINE_MANIFEST_HPP
#define COCAINE_ENGINE_MANIFEST_HPP

#include "cocaine/common.hpp"
#include "cocaine/detail/service/node/cached.hpp"

#include "cocaine/dynamic.hpp"

namespace cocaine { namespace engine {

struct manifest_t:
    cached<dynamic_t>
{
    manifest_t(context_t& context, const std::string& name);

    // The application name.
    std::string name;

    // The application engine's endpoint name.
    std::string endpoint;

    // Optional environment.
    std::map<std::string, std::string> environment;

    // What to execute.
    std::string executable;
};

}} // namespace cocaine::engine

#endif
