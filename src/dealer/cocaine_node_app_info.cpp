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
#include <iomanip>
#include <sstream>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_app_info.hpp"

namespace cocaine {
namespace dealer {

std::ostream& operator << (std::ostream& out, const cocaine_node_app_info_t& info) {
	std::string name = "+ [" + info.name_m + "]";
	std::string running = info.is_running_m ? "yes" : "no";

	std::stringstream slaves;
	slaves << "busy " << info.slaves_busy_m << ", total " << info.slaves_total_m;

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
	out << "queue: " << std::setw(0) << info.queue_depth_m << std::endl;
	
	out << std::setw(2) << "" << std::setw(4) << "" << std::setw(9);
	out << "tasks:" << std::endl;

	cocaine_node_app_info_t::application_tasks::const_iterator it = info.tasks_m.begin();
	for (; it != info.tasks_m.end(); ++it) {
		out << it->second;
	}

	out << std::setw(old_width) << std::setfill(old_char);

	return out;
}

} // namespace dealer
} // namespace cocaine
