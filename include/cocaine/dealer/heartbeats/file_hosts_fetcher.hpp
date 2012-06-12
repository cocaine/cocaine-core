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

#ifndef _COCAINE_DEALER_FILE_HOSTS_FETCHER_HPP_INCLUDED_
#define _COCAINE_DEALER_FILE_HOSTS_FETCHER_HPP_INCLUDED_

#include <vector>

#include "cocaine/dealer/heartbeats/hosts_fetcher_iface.hpp"

namespace cocaine {
namespace dealer {

class file_hosts_fetcher : public hosts_fetcher_iface, private boost::noncopyable  {
public:
	file_hosts_fetcher(const service_info_t& service_info);
	virtual ~file_hosts_fetcher();
	
	bool get_hosts(inetv4_endpoints& endpoints, service_info_t& service_info);

private:
	service_info_t service_info_m;
	time_t file_modification_time_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_FILE_HOSTS_FETCHER_HPP_INCLUDED_
