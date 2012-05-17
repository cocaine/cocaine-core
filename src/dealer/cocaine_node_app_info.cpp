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
#include <iomanip>
#include <sstream>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_app_info.hpp"

namespace cocaine {
namespace dealer {

std::ostream& operator << (std::ostream& out, const cocaine_node_app_info_t& app_info) {
	std::string name = "+ [" + app_info.name + "]";
	std::string running = app_info.is_running ? "yes" : "no";

	std::stringstream slaves;
	slaves << "busy " << app_info.slaves_busy << ", total " << app_info.slaves_total;

	char old_char = out.fill(' ');
	int old_width = out.width(10);

	out << std::left;

	out << std::setw(2) << "";
	out << name << std::endl;

	out << std::setw(2) << "" << std::setw(4) << "" << std::setw(9);
	out << "running: " << std::setw(0) << running << std::endl;

	out << std::setw(2) << "" << std::setw(4) << "" << std::setw(9);
	out << "slaves: " << std::setw(0) << slaves.str() << std::endl;

	out << std::setw(2) << "" << std::setw(4) << "" << std::setw(9);
	out << "queue: " << std::setw(0) << app_info.queue_depth << std::endl;
	
	out << std::setw(2) << "" << std::setw(4) << "" << std::setw(9);
	out << "tasks:" << std::endl;

	cocaine_node_app_info_t::application_tasks::const_iterator it = app_info.tasks.begin();
	for (; it != app_info.tasks.end(); ++it) {
		out << it->second;
	}

	out << std::setw(old_width) << std::setfill(old_char);

	return out;
}

} // namespace dealer
} // namespace cocaine
