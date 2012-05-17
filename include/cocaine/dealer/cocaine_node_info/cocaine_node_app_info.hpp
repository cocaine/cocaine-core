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

#ifndef _COCAINE_DEALER_COCAINE_NODE_APP_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_COCAINE_NODE_APP_INFO_HPP_INCLUDED_

#include <string>
#include <map>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_task_info.hpp"

namespace cocaine {
namespace dealer {

class cocaine_node_app_info_t;
std::ostream& operator << (std::ostream& out, const cocaine_node_app_info_t& app_info);

class cocaine_node_app_info_t {
public:
	// <task name, task info>
	typedef std::map<std::string, cocaine_node_task_info_t> application_tasks;

	cocaine_node_app_info_t() :
		queue_depth(0),
		is_running(false),
		slaves_busy(0),
		slaves_total(0) {};

	explicit cocaine_node_app_info_t(const std::string& app_name) :
		name(app_name),
		queue_depth(0),
		is_running(false),
		slaves_busy(0),
		slaves_total(0) {};

	~cocaine_node_app_info_t() {};

	std::string name;

	application_tasks tasks;

	unsigned int queue_depth;
	bool is_running;
	unsigned int slaves_busy;
	unsigned int slaves_total;

	friend std::ostream& operator << (std::ostream& out, const cocaine_node_app_info_t& app_info);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_APP_INFO_HPP_INCLUDED_
