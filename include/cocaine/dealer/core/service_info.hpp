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

class service_info_t {
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
					  name_(name),
					  description_(description),
					  app_(app),
					  hosts_source_(hosts_source),
					  discovery_type_(discovery_type) {}
	
	bool operator == (const service_info_t& rhs) {
		return (name_ == rhs.name_ &&
				app_ == rhs.app_ &&
				hosts_source_ == rhs.hosts_source_ &&
				discovery_type_ == rhs.discovery_type_);
	}

	std::string as_string() const {
		std::stringstream out;

		out << "service name: " << name_ << "\n";
		out << "description: " << description_ << "\n";
		out << "app: " << app_ << "\n";
		out << "hosts source: " << hosts_source_ << "\n";

		switch (discovery_type_) {
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
	std::string name_;
	std::string description_;
	std::string app_;

	// autodetection
	std::string hosts_source_;
	enum e_autodiscovery_type discovery_type_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SERVICE_INFO_HPP_INCLUDED_
