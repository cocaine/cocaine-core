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

#ifndef _COCAINE_DEALER_HANDLE_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_HANDLE_INFO_HPP_INCLUDED_

#include <string>
#include <map>

#include "cocaine/dealer/structs.hpp"

namespace cocaine {
namespace dealer {

// predeclaration
template <typename LSD_T> class handle_info;
typedef handle_info<DT> handle_info_t;

template <typename LSD_T>
class handle_info {
public:
	handle_info() {};

	handle_info(const std::string& name,
				const std::string& service_name,
				typename LSD_T::port port) :
		name_(name),
		service_name_(service_name),
		port_(port) {};

	handle_info(const handle_info<LSD_T>& h_info) :
		name_(h_info.name_),
		service_name_(h_info.service_name_),
		port_(h_info.port_) {};

	bool operator == (const handle_info<LSD_T>& sh) const {
		return (name_ == sh.name_ &&
				service_name_ == sh.service_name_ &&
				port_ == sh.port_);
	};

	std::string name_;
	std::string service_name_;
	typename LSD_T::port port_;
};

template <typename LSD_T>
std::ostream& operator << (std::ostream& out, const handle_info<LSD_T>& handle) {
	out << "service name: " << handle.service_name_ << " name: " << handle.name_ << ", port: " << handle.port_;

	return out;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HANDLE_INFO_HPP_INCLUDED_
