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
	inetv4_endpoint() : port_(0) {
	}

	explicit inetv4_endpoint(const inetv4_host& host) :
		host_(host),
		port_(0) {}

	inetv4_endpoint(const inetv4_host& host, unsigned short port) :
		host_(host),
		port_(port) {}

	inetv4_endpoint(unsigned int ip, unsigned short port) :
		host_(inetv4_host(ip)),
		port_(port) {}

	inetv4_endpoint(const std::string& ip, const std::string& port) :
		host_(inetv4_host(ip))
	{
		port_ = boost::lexical_cast<unsigned short>(port);
	}

	inetv4_endpoint(const inetv4_endpoint& rhs) :
		host_(rhs.host_),
		port_(rhs.port_)

	{}

	bool operator == (const inetv4_endpoint& rhs) const {
		return (host_ == rhs.host_ && port_ == rhs.port_);
	}

	bool operator != (const inetv4_endpoint& rhs) const {
		return (!(*this == rhs));
	}

	bool operator < (const inetv4_endpoint& rhs) const {
		return (as_string() < rhs.as_string());
	}

	std::string as_string() const {
		return host_.as_string() + ":" + boost::lexical_cast<std::string>(port_);
	}

	inetv4_host host_;
	unsigned short port_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_INETV4_ENDPOINT_HPP_INCLUDED_
