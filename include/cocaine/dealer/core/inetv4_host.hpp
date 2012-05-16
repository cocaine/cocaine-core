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

#ifndef _COCAINE_DEALER_INETV4_HOST_HPP_INCLUDED_
#define _COCAINE_DEALER_INETV4_HOST_HPP_INCLUDED_

#include <string>

#include "cocaine/dealer/utils/networking.hpp"

namespace cocaine {
namespace dealer {

 // predeclaration
class inetv4_host {
public:
	inetv4_host() : ip_(0) {
	}

	explicit inetv4_host(int ip) : ip_(ip) {
		hostname_ = nutils::hostname_for_ipv4(ip);
	}

	explicit inetv4_host(const std::string& ip) {
		ip_ = nutils::str_to_ipv4(ip);
		hostname_ = nutils::hostname_for_ipv4(ip);
	}

	inetv4_host(const inetv4_host& rhs) :
		ip_(rhs.ip_), hostname_(rhs.hostname_) {
	}

	inetv4_host(int ip, const std::string& hostname) :
		ip_(ip), hostname_(hostname) {
	}

	inetv4_host(const std::string& ip, const std::string& hostname) :
		ip_(nutils::str_to_ipv4(ip)), hostname_(hostname) {
	}

	bool operator == (const inetv4_host& rhs) const {
		return (ip_ == rhs.ip_ && hostname_ == rhs.hostname_);
	}

	bool operator != (const inetv4_host& rhs) const {
		return (!(*this == rhs));
	}

	std::string as_string() const {
		return nutils::ipv4_to_str(ip_) + " (" + hostname_ + ")";
	}

	unsigned int ip_;
	std::string hostname_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_INETV4_HOST_HPP_INCLUDED_
