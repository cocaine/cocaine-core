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
