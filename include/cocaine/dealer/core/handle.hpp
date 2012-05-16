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

#ifndef _COCAINE_DEALER_HANDLE_HPP_INCLUDED_
#define _COCAINE_DEALER_HANDLE_HPP_INCLUDED_

#include <string>
#include <map>
#include <memory>
#include <cerrno>

#include <zmq.hpp>

#include <msgpack.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include "json/json.h"

#include "cocaine/dealer/structs.hpp"

#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/core/handle_info.hpp"
#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/core/cached_response.hpp"
#include "cocaine/dealer/core/message_cache.hpp"
#include "cocaine/dealer/core/cocaine_endpoint.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"

namespace cocaine {
namespace dealer {

#define CONTROL_MESSAGE_NOTHING 0
#define CONTROL_MESSAGE_CONNECT 1
#define CONTROL_MESSAGE_RECONNECT 2
#define CONTROL_MESSAGE_DISCONNECT 3
#define CONTROL_MESSAGE_CONNECT_NEW_HOSTS 4
#define CONTROL_MESSAGE_KILL 5

enum e_server_response_code {
	SERVER_RPC_MESSAGE_CHUNK = 5,
	SERVER_RPC_MESSAGE_ERROR = 6,
	SERVER_RPC_MESSAGE_CHOKE = 7
};

// predeclaration
class handle_t : public boost::noncopyable {
public:
	typedef std::vector<cocaine_endpoint> endpoints_list_t;
	typedef boost::shared_ptr<zmq::socket_t> socket_ptr_t;

	typedef boost::shared_ptr<cached_response> cached_response_prt_t;
	typedef boost::function<void(cached_response_prt_t)> responce_callback_t;

public:
	handle_t(const handle_info_t& info,
			 boost::shared_ptr<cocaine::dealer::context> context,
			 const endpoints_list_t& endpoints);

	~handle_t();

	const handle_info_t& info() const;

	// networking
	void connect();
	//void connect(const hosts_info_list_t& hosts);
	//void connect_new_hosts(const hosts_info_list_t& hosts);
	//void reconnect(const hosts_info_list_t& hosts);
	void disconnect();

	// responses consumer
	void set_responce_callback(responce_callback_t callback);

	// message processing
	void enqueue_message(const boost::shared_ptr<message_iface>& message);
	void make_all_messages_new();
	void assign_message_queue(const message_cache::message_queue_ptr_t& message_queue);
	message_cache::message_queue_ptr_t new_messages();

	std::string description();

private:
	void kill();
	void dispatch_messages();
	void log_dispatch_start();

	// working with control messages
	void establish_control_conection(socket_ptr_t& control_socket);
	int receive_control_messages(socket_ptr_t& control_socket, int poll_timeout);
	void dispatch_control_messages(int type, socket_ptr_t& main_socket);
	void reshedule_message(const std::string& uuid);

	// working with messages
	bool dispatch_next_available_message(socket_ptr_t main_socket);
	void process_deadlined_messages();

	// networking
	void recreate_main_socket(socket_ptr_t& main_socket, int timeout, int64_t hwm, const std::string& identity);
	void connect_zmq_socket_to_endpoints(socket_ptr_t& socket,
										 endpoints_list_t& endpoints);

	// working with responces
	bool check_for_responses(socket_ptr_t& main_socket, int poll_timeout);
	bool dispatch_responce(socket_ptr_t& main_socket);
	void process_responce(boost::ptr_vector<zmq::message_t>& chunks);
	bool receive_responce_chunk(socket_ptr_t& socket, zmq::message_t& response);
	void enqueue_response(cached_response_prt_t response);

	// misc
	void log_connection(const std::string prefix, const endpoints_list_t& endpoints);

	// send collected statistics to global stats collector
	handle_stats& get_statistics();
	void update_statistics();

	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config();
	boost::shared_ptr<cocaine::dealer::context> context();

	static const int socket_poll_timeout = 100; // millisecs

private:
	handle_info_t info_;
	boost::shared_ptr<cocaine::dealer::context> context_;
	endpoints_list_t endpoints_;
	//hosts_info_list_t new_hosts_;
	boost::shared_ptr<message_cache> message_cache_;

	boost::thread thread_;
	boost::mutex mutex_;
	volatile bool is_running_;
	volatile bool is_connected_;
	boost::condition_variable cond_;

	std::auto_ptr<zmq::socket_t> zmq_control_socket_;
	bool receiving_control_socket_ok_;

	responce_callback_t response_callback_;

	handle_stats statistics_;

	progress_timer last_response_timer_;
	progress_timer deadlined_messages_timer_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HANDLE_HPP_INCLUDED_
