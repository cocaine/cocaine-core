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

#ifndef _COCAINE_DEALER_COCAINE_NODE_TASK_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_COCAINE_NODE_TASK_INFO_HPP_INCLUDED_

#include <string>
#include <map>

namespace cocaine {
namespace dealer {

class cocaine_node_task_info_t;
std::ostream& operator << (std::ostream& out, const cocaine_node_task_info_t& info);

struct cocaine_node_task_info_t {
	cocaine_node_task_info_t() :
		backlog(0),
		median_processing_time(0.0f),
		median_wait_time(0.0f),
		time_spent_in_queues(0.0f),
		time_spent_on_slaves(0.0f) {}

	explicit cocaine_node_task_info_t(const std::string& task_name) :
		name(task_name),
		backlog(0),
		median_processing_time(0.0f),
		median_wait_time(0.0f),
		time_spent_in_queues(0.0f),
		time_spent_on_slaves(0.0f) {}

	~cocaine_node_task_info_t() {}

	std::string		name;
	unsigned int	backlog;
    std::string		endpoint;
    std::string		route;

	double median_processing_time;
	double median_wait_time;
	double time_spent_in_queues;
	double time_spent_on_slaves;

	friend std::ostream& operator << (std::ostream& out, const cocaine_node_task_info_t& info);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_TASK_INFO_HPP_INCLUDED_
