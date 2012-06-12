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

#include <iostream>
#include <iomanip>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_task_info.hpp"

namespace cocaine {
namespace dealer {

std::ostream& operator << (std::ostream& out, const cocaine_node_task_info_t& info) {
	std::string name = "- {" + info.name + "}";

	char old_char = out.fill(' ');
	int old_width = out.width(10);

	out << std::left;
	out << std::setw(10) << "";
	out << name << std::endl;

	out << std::setw(20) << "" << std::setw(10);
	out << "backlog: " << info.backlog << std::endl;

	out << std::setw(20) << "" << std::setw(10);
	out << "endpoint: " << info.endpoint << std::endl;

	out << std::setw(20) << "" << std::setw(10);
	out << "route: " << info.route << std::endl;

	out << std::fixed << std::setprecision(9);

	out << std::setw(20) << "" << std::setw(24);
	out << "median processing time: " << info.median_processing_time << std::endl;

	out << std::setw(20) << "" << std::setw(24);
	out << "median wait time: " << info.median_wait_time << std::endl;

	out << std::setw(20) << "" << std::setw(24);
	out << "time spent in queues: " << info.time_spent_in_queues << std::endl;

	out << std::setw(20) << "" << std::setw(24);
	out << "time spent on slaves: " << info.time_spent_on_slaves << std::endl;

	out << std::setw(old_width) << std::setfill(old_char);

	return out;
}

} // namespace dealer
} // namespace cocaine
