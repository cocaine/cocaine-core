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

struct cocaine_node_app_info_t;
std::ostream& operator << (std::ostream& out, const cocaine_node_app_info_t& info);

struct cocaine_node_app_info_t {
	// <task name, task info>
	typedef std::map<std::string, cocaine_node_task_info_t> application_tasks;

	cocaine_node_app_info_t() :
		queue_depth(0),
		is_running(false),
		slaves_busy(0),
		slaves_total(0) {}

	explicit cocaine_node_app_info_t(const std::string& name_) :
		name(name_),
		queue_depth(0),
		is_running(false),
		slaves_busy(0),
		slaves_total(0) {}

	~cocaine_node_app_info_t() {}

	std::string			name;
	application_tasks	tasks;
	unsigned int		queue_depth;
	bool				is_running;
	unsigned int		slaves_busy;
	unsigned int		slaves_total;

	friend std::ostream& operator << (std::ostream& out, const cocaine_node_app_info_t& info);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_APP_INFO_HPP_INCLUDED_
