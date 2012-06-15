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

#ifndef _COCAINE_DEALER_RESPONSE_HPP_INCLUDED_
#define _COCAINE_DEALER_RESPONSE_HPP_INCLUDED_

#include <string>

#include <boost/shared_ptr.hpp>

#include <cocaine/dealer/forwards.hpp>
#include <cocaine/dealer/structs.hpp>
#include <cocaine/dealer/utils/data_container.hpp>

namespace cocaine {
namespace dealer {

class response_t {
public:
	response_t(const boost::shared_ptr<dealer_impl_t>& dealer,
             const std::string& uuid, const message_path_t& path);

	virtual ~response_t();

	bool get(data_container* data, double timeout = -1.0);

	void response_callback(const response_data& resp_data,
                           const response_info& resp_info);

private:
	friend class dealer;

	boost::shared_ptr<response_impl_t> m_impl;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_RESPONSE_HPP_INCLUDED_
