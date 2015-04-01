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

#ifndef COCAINE_PROCESS_ISOLATE_ARCHIVE_HPP
#define COCAINE_PROCESS_ISOLATE_ARCHIVE_HPP

#include "cocaine/common.hpp"

struct archive;

namespace cocaine { namespace isolate {

struct archive_error_t:
    public std::runtime_error
{
    explicit
    archive_error_t(archive* source);
};

class archive_t {
    const std::unique_ptr<logging::log_t> m_log;

    archive* m_archive;

public:
    archive_t(context_t& context, const std::string& archive);
   ~archive_t();

    void
    deploy(const std::string& prefix);

public:
    std::string
    type() const;

private:
    static
    void
    extract(archive* source, archive* target);
};

}} // namespace cocaine::isolate

#endif
