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

class cocaine_node_task_info_t;
std::ostream& operator << (std::ostream& out, const cocaine_node_task_info_t& info);

class cocaine_node_task_info_t {
public:
	cocaine_node_task_info_t() :
		backlog_m(0),
		median_processing_time_m(0.0f),
		median_wait_time_m(0.0f),
		time_spent_in_queues_m(0.0f),
		time_spent_on_slaves_m(0.0f) {}

	explicit cocaine_node_task_info_t(const std::string& task_name) :
		name_m(task_name),
		backlog_m(0),
		median_processing_time_m(0.0f),
		median_wait_time_m(0.0f),
		time_spent_in_queues_m(0.0f),
		time_spent_on_slaves_m(0.0f) {}

	~cocaine_node_task_info_t() {}

	std::string name_m;

	unsigned int backlog_m;
    std::string endpoint_m;
    std::string route_m;

	double median_processing_time_m;
	double median_wait_time_m;
	double time_spent_in_queues_m;
	double time_spent_on_slaves_m;

	friend std::ostream& operator << (std::ostream& out, const cocaine_node_task_info_t& info);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_TASK_INFO_HPP_INCLUDED_
