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

void
process_t::spool() {
    std::string blob;

    COCAINE_LOG_INFO(m_log, "deploying the app")("target", m_working_directory);

    auto storage = api::storage(m_context, "core");

    try {
        blob = storage->get<std::string>("apps", m_name);
    } catch(const storage_error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to fetch the app from the storage: %s", e.what());
        throw cocaine::error_t("the '%s' app is not available", m_name);
    }

    try {
        archive_t archive(m_context, blob);

#if BOOST_VERSION >= 104600
        archive.deploy(m_working_directory.native());
#else
        archive.deploy(m_working_directory.string());
#endif
    } catch(const archive_error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to extract the app files: %s", e.what());
        throw cocaine::error_t("the '%s' app is not available", m_name);
    }
}

