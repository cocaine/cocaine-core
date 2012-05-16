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

#ifndef _COCAINE_DEALER_COCAINE_NODE_TASK_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_COCAINE_NODE_TASK_INFO_HPP_INCLUDED_

#include <string>
#include <map>

namespace cocaine {
namespace dealer {

class cocaine_node_task_info;
std::ostream& operator << (std::ostream& out, const cocaine_node_task_info& task_info);

class cocaine_node_task_info {
public:
	cocaine_node_task_info() :
		backlog(0),
		median_processing_time(0.0f),
		median_wait_time(0.0f),
		time_spent_in_queues(0.0f),
		time_spent_on_slaves(0.0f)
		 {};

	explicit cocaine_node_task_info(const std::string& task_name) :
		name(task_name),
		backlog(0),
		median_processing_time(0.0f),
		median_wait_time(0.0f),
		time_spent_in_queues(0.0f),
		time_spent_on_slaves(0.0f)
		 {};

	~cocaine_node_task_info() {};

	std::string name;

	unsigned int backlog;
    std::string endpoint;
    std::string route;

	double median_processing_time;
	double median_wait_time;
	double time_spent_in_queues;
	double time_spent_on_slaves;

	friend std::ostream& operator << (std::ostream& out, const cocaine_node_task_info& task_info);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_TASK_INFO_HPP_INCLUDED_
