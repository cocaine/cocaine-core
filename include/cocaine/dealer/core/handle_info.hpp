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

#include <boost/lexical_cast.hpp>

#include "cocaine/dealer/structs.hpp"

namespace cocaine {
namespace dealer {

class handle_info_t {
public:
	handle_info_t() {};

	handle_info_t(const std::string& name,
				const std::string& service_name) :
		name_(name),
		service_name_(service_name) {};

	handle_info_t(const handle_info_t& rhs) :
		name_(rhs.name_),
		service_name_(rhs.service_name_) {};

	bool operator == (const handle_info_t& rhs) const {
		return (name_ == rhs.name_ &&
				service_name_ == rhs.service_name_);
	};

	std::string as_string() const {
		return "[" + service_name_ + "].[" + name_ + "]";
	}

	std::string name_;
	std::string service_name_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HANDLE_INFO_HPP_INCLUDED_
