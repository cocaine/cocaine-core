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

#include "cocaine/detail/isolates/process.hpp"
#include "cocaine/detail/gateways/adhoc.hpp"

#ifdef COCAINE_ALLOW_RAFT
    #include "cocaine/detail/raft/control_service.hpp"
    #include "cocaine/detail/services/counter.hpp"
#endif

#include "cocaine/detail/services/logging.hpp"
#include "cocaine/detail/services/node.hpp"
#include "cocaine/detail/services/storage.hpp"
#include "cocaine/detail/storages/files.hpp"

#include "cocaine/detail/essentials.hpp"

void
cocaine::essentials::initialize(api::repository_t& repository) {
    repository.insert<isolate::process_t>("process");
    repository.insert<gateway::adhoc_t>("adhoc");

#ifdef COCAINE_ALLOW_RAFT
    repository.insert<raft::control_service_t>("raft");
    repository.insert<service::counter_t>("counter");
#endif

    repository.insert<service::logging_t>("logging");
    repository.insert<service::node_t>("node");
    repository.insert<service::storage_t>("storage");
    repository.insert<storage::files_t>("files");
}
