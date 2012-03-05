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

#ifndef _COCAINE_DEALER_SERVICE_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_SERVICE_INFO_HPP_INCLUDED_

#include <string>
#include <map>

#include "cocaine/dealer/structs.hpp"

namespace cocaine {
namespace dealer {

// predeclaration
template <typename LSD_T> class service_info;
typedef service_info<DT> service_info_t;

template <typename LSD_T>
class service_info {
public:	
	service_info() {};
	service_info(const service_info<LSD_T>& info) {
		*this = info;
	};

	service_info (const std::string& name,
				  const std::string& description,
				  const std::string& app_name,
				  const std::string& instance,
				  const std::string& hosts_url) :
					  name_(name),
					  description_(description),
					  app_name_(app_name),
					  instance_(instance),
					  hosts_url_(hosts_url),
					  control_port_(DEFAULT_CONTROL_PORT) {};
	
	bool operator == (const service_info& rhs) {
		return (name_ == rhs.name_ &&
				hosts_url_ == rhs.hosts_url_ &&
				instance_ == rhs.instance_ &&
				control_port_ == rhs.control_port_);
	};

	// config-defined data
	std::string name_;
	std::string description_;
	std::string app_name_;
	std::string instance_;
	std::string hosts_url_;
	typename LSD_T::port control_port_;
};

template <typename LSD_T>
std::ostream& operator << (std::ostream& out, const service_info<LSD_T>& service_inf) {
	out << "lsd service name: " << service_inf.name_ << "\n";
	out << "description: " << service_inf.description_ << "\n";
	out << "app name: " << service_inf.app_name_ << "\n";
	out << "instance: " << service_inf.instance_ << "\n";
	out << "hosts url: " << service_inf.hosts_url_ << "\n";
	out << "control port: " << service_inf.control_port_ << "\n";

	return out;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SERVICE_INFO_HPP_INCLUDED_
