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
#include "cocaine/dealer/defaults.hpp"

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
				  const std::string& app,
				  const std::string& hosts_source,
				  enum e_autodiscovery_type discovery_type) :
					  name_(name),
					  description_(description),
					  app_(app),
					  hosts_source_(hosts_source),
					  discovery_type_(discovery_type) {};
	
	bool operator == (const service_info& rhs) {
		return (name_ == rhs.name_ &&
				app_ == rhs.app_ &&
				hosts_source_ == rhs.hosts_source_ &&
				discovery_type_ == rhs.discovery_type_);
	};

	// config-defined data
	std::string name_;
	std::string description_;
	std::string app_;

	// autodetection
	std::string hosts_source_;
	enum e_autodiscovery_type discovery_type_;
};

template <typename LSD_T>
std::ostream& operator << (std::ostream& out, const service_info<LSD_T>& service_inf) {
	out << "service name: " << service_inf.name_ << "\n";
	out << "description: " << service_inf.description_ << "\n";
	out << "app: " << service_inf.app_name_ << "\n";
	out << "hosts source: " << service_inf.hosts_source_ << "\n";

	switch (service_inf.discovery_type_) {
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

	return out;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SERVICE_INFO_HPP_INCLUDED_
