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

#ifndef _COCAINE_DEALER_BALANCER_HPP_INCLUDED_
#define _COCAINE_DEALER_BALANCER_HPP_INCLUDED_

#include <vector>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <zmq.hpp>

#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/core/message_cache.hpp"
#include "cocaine/dealer/core/dealer_object.hpp"
#include "cocaine/dealer/core/cached_response.hpp"
#include "cocaine/dealer/core/cocaine_endpoint.hpp"

namespace cocaine {
namespace dealer {

class balancer_t : private boost::noncopyable, public dealer_object_t {
public:
	balancer_t(const std::string& identity,
			   const std::vector<cocaine_endpoint>& endpoints,
			   boost::shared_ptr<message_cache> message_cache,
			   const boost::shared_ptr<context_t>& ctx,
			   bool logging_enabled = true);

	virtual ~balancer_t();

	void connect(const std::vector<cocaine_endpoint>& endpoints);
	void disconnect();

	bool send(boost::shared_ptr<message_iface>& message, cocaine_endpoint& endpoint);
	bool receive(boost::shared_ptr<cached_response_t>& response);

	void update_endpoints(const std::vector<cocaine_endpoint>& endpoints,
						  std::vector<cocaine_endpoint>& missing_endpoints);

	bool check_for_responses(int poll_timeout) const;

	static const int socket_timeout = 0;
	static const int64_t socket_hwm = 0;

private:
	void get_endpoints_diff(const std::vector<cocaine_endpoint>& updated_endpoints,
							std::vector<cocaine_endpoint>& new_endpoints,
							std::vector<cocaine_endpoint>& missing_endpoints);

	void recreate_socket();
	
	bool receive_chunk(zmq::message_t& response);
	bool process_responce(boost::ptr_vector<zmq::message_t>& chunks,
						  boost::shared_ptr<cached_response_t>& response);

	cocaine_endpoint& get_next_endpoint();

private:
	boost::shared_ptr<zmq::socket_t>	socket_m;
	std::vector<cocaine_endpoint>		endpoints_m;
	boost::shared_ptr<message_cache>	message_cache_m;
	size_t		current_endpoint_index_m;
	std::string	socket_identity_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_BALANCER_HPP_INCLUDED_
