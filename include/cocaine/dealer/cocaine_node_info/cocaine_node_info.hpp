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

class cocaine_node_info_t;
std::ostream& operator << (std::ostream& out, const cocaine_node_info_t& node_info);

struct cocaine_node_info_t {
	// <app name, app info>
	typedef std::map<std::string, cocaine_node_app_info_t> applications;

	cocaine_node_info_t() : 
		pending_jobs(0),
		processed_jobs(0),
		uptime(0.0f),
		ip_address(0),
		port(0) {};

	~cocaine_node_info_t() {};

	bool app_by_name(const std::string& name, cocaine_node_app_info_t& app) const {
		applications::const_iterator it = apps.find(name);

		if (it != apps.end()) {
			app = it->second;
			return true;
		}

		return false;
	}

	applications	apps;
	unsigned int	pending_jobs;
	unsigned int	processed_jobs;
	std::string		route;
	double			uptime;
	unsigned int	ip_address;
	unsigned short	port;

	friend std::ostream& operator << (std::ostream& out, const cocaine_node_info_t& node_info);
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_INFO_HPP_INCLUDED_
