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

#include "cocaine/detail/drivers/fs.hpp"
#include "cocaine/detail/drivers/time.hpp"
#include "cocaine/detail/isolates/process.hpp"
#include "cocaine/detail/gateways/adhoc.hpp"
#include "cocaine/detail/loggers/files.hpp"
#include "cocaine/detail/loggers/syslog.hpp"
#include "cocaine/detail/services/logging.hpp"
#include "cocaine/detail/services/node.hpp"
#include "cocaine/detail/services/storage.hpp"
#include "cocaine/detail/storages/files.hpp"

#include "cocaine/detail/essentials.hpp"

void
cocaine::essentials::initialize(api::repository_t& repository) {
    repository.insert<driver::fs_t>("fs");
    repository.insert<driver::recurring_timer_t>("time");
    repository.insert<isolate::process_t>("process");
    repository.insert<gateway::adhoc_t>("adhoc");
    repository.insert<logger::files_t>("files");
    repository.insert<logger::syslog_t>("syslog");
    repository.insert<service::file_logger_t>("filelogger");
    repository.insert<service::logging_t>("logging");
    repository.insert<service::node_t>("node");
    repository.insert<service::storage_t>("storage");
    repository.insert<storage::files_t>("files");
}
