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

#include "cocaine/essentials/module.hpp"
#include "cocaine/essentials/isolates/process.hpp"
#include "cocaine/essentials/loggers/files.hpp"
#include "cocaine/essentials/loggers/stdout.hpp"
#include "cocaine/essentials/loggers/syslog.hpp"
#include "cocaine/essentials/services/logging.hpp"
#include "cocaine/essentials/services/node.hpp"
#include "cocaine/essentials/services/storage.hpp"
#include "cocaine/essentials/storages/files.hpp"

void
cocaine::essentials::initialize(api::repository_t& repository) {
    repository.insert<isolate::process_t>("process");
    repository.insert<logger::files_t>("files");
    repository.insert<logger::stdout_t>("stdout");
    repository.insert<logger::syslog_t>("syslog");
    repository.insert<service::logging_t>("logging");
    repository.insert<service::node_t>("node");
    repository.insert<service::storage_t>("storage");
    repository.insert<storage::files_t>("files");
}
