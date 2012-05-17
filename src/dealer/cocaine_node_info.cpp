//
// Copyright (C) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
