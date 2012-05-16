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

#ifndef _COCAINE_DEALER_NETWORKING_HPP_INCLUDED_
#define _COCAINE_DEALER_NETWORKING_HPP_INCLUDED_

#include <string>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "cocaine/dealer/utils/error.hpp"

namespace cocaine {
namespace dealer {

class nutils {
public:
	static int str_to_ipv4(const std::string& str) {
		int addr;
        int res = inet_pton(AF_INET, str.c_str(), &addr);

        if (0 == res) {
			throw internal_error(std::string("bad ip address: ") + str);
        }
        else if (res < 0) {
			throw internal_error("bad address translation");
        }

        return htonl(addr);
	}

	static std::string ipv4_to_str(int ip) {
		char buf[128];
        int addr = ntohl(ip);
        return inet_ntop(AF_INET, &addr, buf, sizeof(buf));
	}

	static std::string hostname_for_ipv4(const std::string& ip) {
		in_addr_t data = inet_addr(ip.c_str());
		const hostent* host_info = gethostbyaddr(&data, 4, AF_INET);

		if (host_info) {
			return std::string(host_info->h_name);
		}

		return "";
	}

	static std::string hostname_for_ipv4(int ip) {
		return hostname_for_ipv4(ipv4_to_str(ip));
	}
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_NETWORKING_HPP_INCLUDED_
