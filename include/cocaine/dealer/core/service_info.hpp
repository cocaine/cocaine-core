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

#ifndef _COCAINE_DEALER_SERVICE_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_SERVICE_INFO_HPP_INCLUDED_

#include <string>
#include <sstream>
#include <map>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/defaults.hpp"

namespace cocaine {
namespace dealer {

struct service_info_t {
public:	
	service_info_t() {};
	service_info_t(const service_info_t& info) {
		*this = info;
	}

	service_info_t(const std::string& name,
				  const std::string& description,
				  const std::string& app,
				  const std::string& hosts_source,
				  enum e_autodiscovery_type discovery_type) :
					  name(name),
					  description(description),
					  app(app),
					  hosts_source(hosts_source),
					  discovery_type(discovery_type) {}
	
	bool operator == (const service_info_t& rhs) {
		return (name == rhs.name &&
				app == rhs.app &&
				hosts_source == rhs.hosts_source &&
				discovery_type == rhs.discovery_type);
	}

	std::string as_string() const {
		std::stringstream out;

		out << "service name: " << name << "\n";
		out << "description: " << description << "\n";
		out << "app: " << app << "\n";
		out << "hosts source: " << hosts_source << "\n";

		switch (discovery_type) {
			case AT_MULTICAST:
				out << "discovery type: multicast\n";
				break;

			case AT_HTTP:
				out << "discovery type: http\n";
				break;

			case AT_FILE:
				out << "discovery type: file\n";
				break;
		}

		return out.str();
	}

	// config-defined data
	std::string name;
	std::string description;
	std::string app;

	// autodetection
	std::string hosts_source;
	enum e_autodiscovery_type discovery_type;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SERVICE_INFO_HPP_INCLUDED_
