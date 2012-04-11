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

#ifndef _COCAINE_DEALER_HTTP_HOSTS_FETCHER_HPP_INCLUDED_
#define _COCAINE_DEALER_HTTP_HOSTS_FETCHER_HPP_INCLUDED_

#include <string>
#include <vector>

#include <boost/cstdint.hpp>
#include <curl/curl.h>

#include "cocaine/dealer/core/host_info.hpp"
#include "cocaine/dealer/core/service_info.hpp"

namespace cocaine {
namespace dealer {

class http_hosts_fetcher : private boost::noncopyable  {
public:
	http_hosts_fetcher(service_info_t service_info);
	virtual ~http_hosts_fetcher();

	void get_hosts(std::vector<host_info_t>& hosts, service_info_t& service_info);

private:
	static int curl_writer(char* data, size_t size, size_t nmemb, std::string* buffer_in);

private:
	CURL* curl_;
	service_info_t service_info_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CURL_HOSTS_FETCHER_HPP_INCLUDED_
