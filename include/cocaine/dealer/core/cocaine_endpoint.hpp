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

#ifndef _COCAINE_DEALER_COCAINE_ENDPOINT_HPP_INCLUDED_
#define _COCAINE_DEALER_COCAINE_ENDPOINT_HPP_INCLUDED_

#include <string>

namespace cocaine {
namespace dealer {

 // predeclaration
class cocaine_endpoint {
public:
	cocaine_endpoint(const std::string& endpoint, const std::string& route) :
		endpoint_(endpoint),
		route_(route) {}

	cocaine_endpoint() {}
	~cocaine_endpoint() {}

	cocaine_endpoint(const cocaine_endpoint& rhs) :
		endpoint_(rhs.endpoint_),
		route_(rhs.route_) {}

	cocaine_endpoint& operator = (const cocaine_endpoint& rhs) {
		if (this != &rhs) {
			endpoint_ = rhs.endpoint_;
			route_ = rhs.route_;
		}

		return *this;
	}

	bool operator == (const cocaine_endpoint& rhs) const {
		return (endpoint_ == rhs.endpoint_ &&
				route_ == rhs.route_);
	}

	bool operator != (const cocaine_endpoint& rhs) const {
		return (!(*this == rhs));
	}

	bool operator < (const cocaine_endpoint& rhs) const {
		return ((endpoint_ + route_) < (rhs.endpoint_ + rhs.route_));
	}

	std::string as_string() const {
		return "endpoint: " + endpoint_ + ", route: " + route_;
	}

	std::string endpoint_;
	std::string route_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_ENDPOINT_HPP_INCLUDED_
