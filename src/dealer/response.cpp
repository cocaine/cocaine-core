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

#include <stdexcept>

#include <cocaine/dealer/response.hpp>
#include <cocaine/dealer/core/dealer_impl.hpp>
#include <cocaine/dealer/core/response_impl.hpp>

namespace cocaine {
namespace dealer {

response::response(const boost::shared_ptr<dealer_impl_t>& dealer, const std::string& uuid, const message_path& path) {
	m_impl.reset(new response_impl_t(dealer, uuid, path));
}

response::~response() {
	m_impl.reset();
}

bool
response::get(data_container* data, double timeout) {
	return m_impl->get(data, timeout);
}

void
response::response_callback(const response_data& resp_data, const response_info& resp_info) {
	m_impl->response_callback(resp_data, resp_info);
}

} // namespace dealer
} // namespace cocaine
