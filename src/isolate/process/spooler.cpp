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

#include "cocaine/detail/isolate/process.hpp"

#include "cocaine/api/storage.hpp"

#include "cocaine/detail/isolate/archive.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::isolate;

struct null_cancellation_t:
    public api::cancellation_t
{
    void
    cancel() {}
};

std::unique_ptr<api::cancellation_t>
process_t::spool(callback_type cb) {
    std::unique_ptr<api::cancellation_t> cancellation(new null_cancellation_t);

    std::string blob;

    try {
        COCAINE_LOG_INFO(m_log, "deploying app to %s", m_working_directory);

        const auto storage = api::storage(m_context, "core");
        const auto archive = storage->get<std::string>("apps", m_name);

    #if BOOST_VERSION >= 104600
        archive_t(m_context, archive).deploy(m_working_directory.native());
    #else
        archive_t(m_context, archive).deploy(m_working_directory.string());
    #endif
    } catch (const std::system_error& err) {
        cb(err.code());
    }

    return cancellation;
}

