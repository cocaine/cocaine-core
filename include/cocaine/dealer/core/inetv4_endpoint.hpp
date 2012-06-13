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

#ifndef _COCAINE_DEALER_INETV4_ENDPOINT_HPP_INCLUDED_
#define _COCAINE_DEALER_INETV4_ENDPOINT_HPP_INCLUDED_

#include <string>

#include "boost/lexical_cast.hpp"

#include "cocaine/dealer/core/inetv4_host.hpp"

namespace cocaine {
namespace dealer {

 // predeclaration
class inetv4_endpoint {
public:
	inetv4_endpoint() : port(0) {
	}

	explicit inetv4_endpoint(const inetv4_host& host) :
		host(host),
		port(0) {}

	inetv4_endpoint(const inetv4_host& host, unsigned short port) :
		host(host),
		port(port) {}

	inetv4_endpoint(unsigned int ip, unsigned short port) :
		host(inetv4_host(ip)),
		port(port) {}

	inetv4_endpoint(const std::string& ip, const std::string& port) :
		host(inetv4_host(ip))
	{
		//port = boost::lexical_cast<unsigned short>(port);
	}

	inetv4_endpoint(const inetv4_endpoint& rhs) :
		host(rhs.host),
		port(rhs.port) {}

	bool operator == (const inetv4_endpoint& rhs) const {
		return (host == rhs.host && port == rhs.port);
	}

	bool operator != (const inetv4_endpoint& rhs) const {
		return (!(*this == rhs));
	}

	bool operator < (const inetv4_endpoint& rhs) const {
		return (as_string() < rhs.as_string());
	}

	std::string as_string() const {
		return host.as_string() + ":" + boost::lexical_cast<std::string>(port);
	}

	inetv4_host host;
	unsigned short port;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_INETV4_ENDPOINT_HPP_INCLUDED_
