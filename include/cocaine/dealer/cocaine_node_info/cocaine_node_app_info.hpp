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

#ifndef _COCAINE_DEALER_COCAINE_NODE_APP_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_COCAINE_NODE_APP_INFO_HPP_INCLUDED_

#include <string>
#include <map>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_task_info.hpp"

namespace cocaine {
namespace dealer {

class cocaine_node_app_info_t;
std::ostream& operator << (std::ostream& out, const cocaine_node_app_info_t& info);

class cocaine_node_app_info_t {
public:
	// <task name, task info>
	typedef std::map<std::string, cocaine_node_task_info_t> application_tasks;

	cocaine_node_app_info_t() :
		queue_depth_m(0),
		is_running_m(false),
		slaves_busy_m(0),
		slaves_total_m(0) {}

	explicit cocaine_node_app_info_t(const std::string& name) :
		name_m(name),
		queue_depth_m(0),
		is_running_m(false),
		slaves_busy_m(0),
		slaves_total_m(0) {}

	~cocaine_node_app_info_t() {}

	std::string			name_m;
	application_tasks	tasks_m;
	unsigned int		queue_depth_m;
	bool				is_running_m;
	unsigned int		slaves_busy_m;
	unsigned int		slaves_total_m;

	friend std::ostream& operator << (std::ostream& out, const cocaine_node_app_info_t& info);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_APP_INFO_HPP_INCLUDED_
