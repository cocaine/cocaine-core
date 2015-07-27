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

#include "cocaine/detail/essentials.hpp"

#include "cocaine/detail/cluster/multicast.hpp"
#include "cocaine/detail/cluster/predefine.hpp"
#include "cocaine/detail/gateway/adhoc.hpp"
#include "cocaine/detail/service/locator.hpp"
#include "cocaine/detail/service/logging.hpp"
#include "cocaine/detail/service/storage.hpp"
#include "cocaine/detail/storage/files.hpp"

void
cocaine::essentials::initialize(api::repository_t& repository) {
    repository.insert<cluster::multicast_t>("multicast");
    repository.insert<cluster::predefine_t>("predefine");
    repository.insert<gateway::adhoc_t>("adhoc");
    repository.insert<service::locator_t>("locator");
    repository.insert<service::logging_t>("logging");
    repository.insert<service::storage_t>("storage");
    repository.insert<storage::files_t>("files");
}
