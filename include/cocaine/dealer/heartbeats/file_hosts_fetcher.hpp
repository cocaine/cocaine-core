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

#ifndef _COCAINE_DEALER_FILE_HOSTS_FETCHER_HPP_INCLUDED_
#define _COCAINE_DEALER_FILE_HOSTS_FETCHER_HPP_INCLUDED_

#include <vector>

#include "cocaine/dealer/heartbeats/hosts_fetcher_iface.hpp"

namespace cocaine {
namespace dealer {

class file_hosts_fetcher_t : public hosts_fetcher_iface, private boost::noncopyable  {
public:
	file_hosts_fetcher_t(const service_info_t& service_info);
	virtual ~file_hosts_fetcher_t();
	
	bool get_hosts(inetv4_endpoints_t& endpoints, service_info_t& service_info);

private:
	service_info_t m_service_info;
	time_t         m_file_modification_time;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_FILE_HOSTS_FETCHER_HPP_INCLUDED_
