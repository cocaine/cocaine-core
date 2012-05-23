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

#ifndef _COCAINE_DEALER_BALANCER_HPP_INCLUDED_
#define _COCAINE_DEALER_BALANCER_HPP_INCLUDED_

#include <vector>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <zmq.hpp>

#include "cocaine/dealer/core/cocaine_endpoint.hpp"
#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/core/message_cache.hpp"
#include "cocaine/dealer/core/cached_response.hpp"

namespace cocaine {
namespace dealer {

class balancer_t : private boost::noncopyable {
public:
	balancer_t(const std::string& identity,
			   const std::vector<cocaine_endpoint>& endpoints,
			   const boost::shared_ptr<cocaine::dealer::context>& context,
			   boost::shared_ptr<message_cache> message_cache);

	virtual ~balancer_t();

	void connect(const std::vector<cocaine_endpoint>& endpoints);
	void disconnect();

	bool send(boost::shared_ptr<message_iface>& message);
	bool receive(boost::shared_ptr<cached_response_t>& response);

	void update_endpoints(const std::vector<cocaine_endpoint>& endpoints,
						  std::vector<cocaine_endpoint>& missing_endpoints);

	bool check_for_responses(int poll_timeout);

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

	std::string get_next_route();

	boost::shared_ptr<base_logger> logger();

private:
	boost::shared_ptr<zmq::socket_t> socket_;
	std::string socket_identity_;
	std::vector<cocaine_endpoint> endpoints_;
	boost::shared_ptr<cocaine::dealer::context> context_;
	boost::shared_ptr<message_cache> message_cache_;
	size_t current_endpoint_index_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_BALANCER_HPP_INCLUDED_
