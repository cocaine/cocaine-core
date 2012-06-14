/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
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

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_info.hpp"
#include "cocaine/dealer/utils/networking.hpp"

namespace cocaine {
namespace dealer {

std::ostream& operator << (std::ostream& out, const cocaine_node_info_t& node_info) {
	std::stringstream pending;
	pending << "pending " << node_info.pending_jobs;
	pending << ", processed " << node_info.processed_jobs;

	std::stringstream host_info;
	host_info << nutils::ipv4_to_str(node_info.ip_address) << ":" << node_info.port;

	char old_char = out.fill(' ');
	int old_width = out.width(10);

	out << std::left;
	out << std::setw(12) << "apps:" << std::endl;

	cocaine_node_info_t::applications::const_iterator it = node_info.apps.begin();
	for (; it != node_info.apps.end(); ++it) {
		out << it->second;
	}

	out << std::setw(12) << "jobs:";
	out << std::setw(50) << pending.str() << std::endl;

	out << std::setw(12) << "route:";
	out << std::setw(50) << node_info.route << std::endl;

	out << std::setw(12) << "uptime:";
	out << std::setw(50) << std::fixed << std::setprecision(11) << node_info.uptime << std::endl;

	out << std::setw(12) << "host info:";
	out << std::setw(50) << host_info.str() << std::endl;

	out << std::setw(old_width) << std::setfill(old_char);

	return out;
};

} // namespace dealer
} // namespace cocaine
