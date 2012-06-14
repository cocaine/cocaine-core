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

#ifndef _COCAINE_DEALER_HTTP_HOSTS_FETCHER_HPP_INCLUDED_
#define _COCAINE_DEALER_HTTP_HOSTS_FETCHER_HPP_INCLUDED_

#include <string>
#include <vector>

#include <curl/curl.h>

#include "cocaine/dealer/heartbeats/hosts_fetcher_iface.hpp"

namespace cocaine {
namespace dealer {

class http_hosts_fetcher : public hosts_fetcher_iface, private boost::noncopyable  {
public:
	http_hosts_fetcher(const service_info_t& service_info);
	virtual ~http_hosts_fetcher();

	bool get_hosts(inetv4_endpoints& endpoints, service_info_t& service_info);

private:
	static int curl_writer(char* data, size_t size, size_t nmemb, std::string* buffer_in);

private:
	CURL* curl_m;
	service_info_t service_info_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CURL_HOSTS_FETCHER_HPP_INCLUDED_
