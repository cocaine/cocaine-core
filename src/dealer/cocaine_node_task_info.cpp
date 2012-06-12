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

#include <iostream>
#include <iomanip>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_task_info.hpp"

namespace cocaine {
namespace dealer {

std::ostream& operator << (std::ostream& out, const cocaine_node_task_info_t& info) {
	std::string name = "- {" + info.name_m + "}";

	char old_char = out.fill(' ');
	int old_width = out.width(10);

	out << std::left;
	out << std::setw(10) << "";
	out << name << std::endl;

	out << std::setw(20) << "" << std::setw(10);
	out << "backlog: " << info.backlog_m << std::endl;

	out << std::setw(20) << "" << std::setw(10);
	out << "endpoint: " << info.endpoint_m << std::endl;

	out << std::setw(20) << "" << std::setw(10);
	out << "route: " << info.route_m << std::endl;

	out << std::fixed << std::setprecision(9);

	out << std::setw(20) << "" << std::setw(24);
	out << "median processing time: " << info.median_processing_time_m << std::endl;

	out << std::setw(20) << "" << std::setw(24);
	out << "median wait time: " << info.median_wait_time_m << std::endl;

	out << std::setw(20) << "" << std::setw(24);
	out << "time spent in queues: " << info.time_spent_in_queues_m << std::endl;

	out << std::setw(20) << "" << std::setw(24);
	out << "time spent on slaves: " << info.time_spent_on_slaves_m << std::endl;

	out << std::setw(old_width) << std::setfill(old_char);

	return out;
}

} // namespace dealer
} // namespace cocaine
