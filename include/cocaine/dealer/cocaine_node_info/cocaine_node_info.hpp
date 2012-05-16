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

#ifndef _COCAINE_DEALER_COCAINE_NODE_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_COCAINE_NODE_INFO_HPP_INCLUDED_

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_app_info.hpp"
#include "cocaine/dealer/utils/networking.hpp"

namespace cocaine {
namespace dealer {

class cocaine_node_info;
std::ostream& operator << (std::ostream& out, const cocaine_node_info& node_info);

class cocaine_node_info {
public:
	// <app name, app info>
	typedef std::map<std::string, cocaine_node_app_info> applications;

	cocaine_node_info() : 
		pending_jobs(0),
		processed_jobs(0),
		uptime(0.0f),
		ip_address(0),
		port(0) {};

	~cocaine_node_info() {};

	bool app_by_name(const std::string& name, cocaine_node_app_info& app) const {
		applications::const_iterator it = apps.find(name);

		if (it != apps.end()) {
			app = it->second;
			return true;
		}

		return false;
	}

	applications apps;
	
	unsigned int pending_jobs;
	unsigned int processed_jobs;
	std::string route;
	double uptime;

	unsigned int ip_address;
	unsigned short port;

	friend std::ostream& operator << (std::ostream& out, const cocaine_node_info& node_info);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_INFO_HPP_INCLUDED_
