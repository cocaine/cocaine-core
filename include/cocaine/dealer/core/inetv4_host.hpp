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
	inetv4_host() : ip(0) {
	}

	explicit inetv4_host(int ip) : ip(ip) {
		hostname = nutils::hostname_for_ipv4(ip);
	}

	explicit inetv4_host(const std::string& ip) {
		ip = nutils::str_to_ipv4(ip);
		hostname = nutils::hostname_for_ipv4(ip);
	}

	inetv4_host(const inetv4_host& rhs) :
		ip(rhs.ip), hostname(rhs.hostname) {
	}

	inetv4_host(int ip, const std::string& hostname) :
		ip(ip), hostname(hostname) {
	}

	inetv4_host(const std::string& ip, const std::string& hostname) :
		ip(nutils::str_to_ipv4(ip)), hostname(hostname) {
	}

	bool operator == (const inetv4_host& rhs) const {
		return (ip == rhs.ip && hostname == rhs.hostname);
	}

	bool operator != (const inetv4_host& rhs) const {
		return (!(*this == rhs));
	}

	std::string as_string() const {
		return nutils::ipv4_to_str(ip) + " (" + hostname + ")";
	}

	unsigned int ip;
	std::string hostname;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_INETV4_HOST_HPP_INCLUDED_
