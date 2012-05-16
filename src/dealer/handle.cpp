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

#include "cocaine/dealer/core/handle.hpp"

namespace cocaine {
namespace dealer {

handle_t::handle_t(const handle_info_t& info,
					  boost::shared_ptr<cocaine::dealer::context> lsd_context,
					  const endpoints_list_t& endpoints) :
	info_(info),
	context_(lsd_context),
	endpoints_(endpoints),
	is_running_(false),
	is_connected_(false),
	receiving_control_socket_ok_(false)
{
	boost::mutex::scoped_lock lock(mutex_);

	logger()->log(PLOG_DEBUG, "CREATED HANDLE [%s].[%s]", info.service_name_.c_str(), info.name_.c_str());

	// create message cache
	message_cache_.reset(new message_cache(context(), config()->message_cache_type()));

	// create control socket
	std::string conn_str = "inproc://service_control_" + info_.service_name_ + "_" + info_.name_;
	zmq_control_socket_.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_PAIR));

	int timeout = 0;
	zmq_control_socket_->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
	zmq_control_socket_->bind(conn_str.c_str());

	// run message dispatch thread
	is_running_ = true;
	thread_ = boost::thread(&handle_t::dispatch_messages, this);

	// connect to hosts 
	lock.unlock();
	connect();
}

handle_t::~handle_t() {
	kill();

	zmq_control_socket_->close();
	zmq_control_socket_.reset(NULL);

	thread_.join();

	logger()->log(PLOG_DEBUG, "KILLED HANDLE [%s].[%s]", info_.service_name_.c_str(), info_.name_.c_str());
}

void
handle_t::dispatch_messages() {
	// establish connections
	socket_ptr_t main_socket;
	socket_ptr_t control_socket;

	establish_control_conection(control_socket);

	std::string log_str = "started message dispatch for [%s].[%s]";
	logger()->log(PLOG_DEBUG, log_str.c_str(), info_.service_name_.c_str(), info_.name_.c_str());

	last_response_timer_.reset();
	deadlined_messages_timer_.reset();

	// process messages
	while (is_running_) {
		boost::mutex::scoped_lock lock(mutex_);

		static bool have_enqueued_messages = false;

		// process incoming control messages
		int control_message = receive_control_messages(control_socket, 0);
		if (control_message == CONTROL_MESSAGE_KILL) {
			// stop message dispatch, finalize everything
			is_running_ = false;
			break;
		}
		else if (control_message > 0) {
			dispatch_control_messages(control_message, main_socket);
			logger()->log(PLOG_DEBUG, "DONE PROCESSING CONTROL msgs: %d", message_cache_->new_messages_count());
		}

		// send new message if any
		if (is_running_ && is_connected_) {
			for (int i = 0; i < 100; ++i) { // batching
				if (message_cache_->new_messages_count() == 0) {
					break;	
				}

				dispatch_next_available_message(main_socket);
			}
		}

		// check for message responces
		bool received_response = false;

		int fast_poll_timeout = 10;		// microsecs
		int long_poll_timeout = 1000;	// microsecs

		int response_poll_timeout = fast_poll_timeout;
		if (last_response_timer_.elapsed() > 0.5f) {	// 0.5f == 500 millisecs
			response_poll_timeout = long_poll_timeout;			
		}

		if (is_connected_ && is_running_) {
			lock.unlock();
			received_response = check_for_responses(main_socket, response_poll_timeout);
			lock.lock();

			// process received responce(s)
			while (received_response) {
				last_response_timer_.reset();
				response_poll_timeout = fast_poll_timeout;

				lock.unlock();
				dispatch_responce(main_socket);
				lock.lock();

				lock.unlock();
				received_response = check_for_responses(main_socket, fast_poll_timeout);
				lock.lock();
			}
		}

		if (is_running_) {
			if (deadlined_messages_timer_.elapsed() > 0.01f) {	// 0.01f == 10 millisecs
				lock.unlock();
				process_deadlined_messages();
				lock.lock();
				deadlined_messages_timer_.reset();
			}
		}
	}

	control_socket.reset();
	main_socket.reset();

	update_statistics();
	log_str = "finished message dispatch for [%s].[%s]";
	logger()->log(PLOG_DEBUG, log_str.c_str(), info_.service_name_.c_str(), info_.name_.c_str());
}

void
handle_t::process_deadlined_messages() {
	assert(message_cache_);
	message_cache::expired_messages_data_t expired_messages;
	message_cache_->get_expired_messages(expired_messages);

	if (expired_messages.empty()) {
		return;
	}

	for (size_t i = 0; i < expired_messages.size(); ++i) {
		cached_response_prt_t new_response;
		new_response.reset(new cached_response(expired_messages.at(i).first,
											   expired_messages.at(i).second,
											   deadline_error,
											   "message expired"));
		enqueue_response(new_response);
	}
}

void
handle_t::establish_control_conection(socket_ptr_t& control_socket) {
	control_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_PAIR));

	if (control_socket.get()) {
		std::string conn_str = "inproc://service_control_" + info_.service_name_ + "_" + info_.name_;

		int timeout = 0;
		control_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
		control_socket->connect(conn_str.c_str());
		receiving_control_socket_ok_ = true;
	}
}

void
handle_t::enqueue_response(cached_response_prt_t response) {
	if (response_callback_ && is_running_) {
		response_callback_(response);
	}
}

int
handle_t::receive_control_messages(socket_ptr_t& control_socket, int poll_timeout) {
	if (!is_running_) {
		return 0;
	}

	// poll for responce
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *control_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	int socket_response = zmq_poll(poll_items, 1, poll_timeout);

	if (socket_response <= 0) {
		return 0;
	}

	// in case we received control message
    if ((ZMQ_POLLIN & poll_items[0].revents) != ZMQ_POLLIN) {
    	return 0;
    }

	int received_message = 0;

	bool recv_failed = false;
	zmq::message_t reply;

	try {
		if (!control_socket->recv(&reply)) {
			recv_failed = true;
		}
		else {
			memcpy((void *)&received_message, reply.data(), reply.size());
			return received_message;
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "some very ugly shit happend while recv on socket at ";
		error_msg += std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " details: " + std::string(ex.what());
		throw internal_error(error_msg);
	}

    if (recv_failed) {
    	std::string sname = info_.service_name_;
    	std::string hname = info_.name_;
    	logger()->log("recv failed on service: %s, handle %s", sname.c_str(), hname.c_str());
    }

    return 0;
}

void
handle_t::recreate_main_socket(socket_ptr_t& main_socket, int timeout, int64_t hwm, const std::string& identity) {
	main_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_ROUTER));
	main_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
	main_socket->setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
	main_socket->setsockopt(ZMQ_IDENTITY, identity.c_str(), identity.length());
}

void
handle_t::dispatch_control_messages(int type, socket_ptr_t& main_socket) {
	if (!is_running_) {
		return;
	}

	switch (type) {
		case CONTROL_MESSAGE_CONNECT:
			//logger()->log(PLOG_DEBUG, "CONTROL_MESSAGE_CONNECT");

			// create new main socket in case we're not connected
			if (!is_connected_) {
				recreate_main_socket(main_socket, 500, 0, "huita");
				connect_zmq_socket_to_endpoints(main_socket, endpoints_);
				is_connected_ = true;
			}
			break;

		/*
		case CONTROL_MESSAGE_RECONNECT:
			logger()->log(PLOG_DEBUG, "CONTROL_MESSAGE_RECONNECT - DO");

			recreate_main_socket(main_socket, 500, 0, "huita");
			connect_zmq_socket_to_endpoints(main_socket, endpoints_);
			is_connected_ = true;
			logger()->log(PLOG_DEBUG, "CONTROL_MESSAGE_RECONNECT - DONE");
			break;

		case CONTROL_MESSAGE_DISCONNECT:
			//logger()->log(PLOG_DEBUG, "CONTROL_MESSAGE_DISCONNECT");

			// kill socket
			main_socket.reset();
			is_connected_ = false;
			break;

		case CONTROL_MESSAGE_CONNECT_NEW_HOSTS:
			//logger()->log(PLOG_DEBUG, "CONTROL_MESSAGE_CONNECT_NEW_HOSTS");

			// connect socket to new hosts
			hosts_info_list_t new_hosts;
			if (is_connected_ && !new_hosts_.empty()) {
				new_hosts = new_hosts_;
				new_hosts_.clear();

				if (!new_hosts.empty()) {
					connect_zmq_socket_to_endpoints(main_socket, endpoints_);
				}
			}
			break;
		*/
	}
}

void
handle_t::connect_zmq_socket_to_endpoints(socket_ptr_t& socket,
										   	   endpoints_list_t& endpoints)
{
	if (endpoints.empty()) {
		return;
	}

	assert(socket);

	// connect socket to hosts
	std::string connection_str;
	try {
		for (size_t i = 0; i < endpoints.size(); ++i) {
			connection_str = endpoints[i].endpoint_; 
			socket->connect(connection_str.c_str());
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = info_.as_string() + " could not connect to ";
		error_msg += connection_str + " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}
}

bool
handle_t::dispatch_next_available_message(socket_ptr_t main_socket) {
	// validate socket
	if (!main_socket) {
		std::string error_msg = "service: " + info_.service_name_;
		error_msg += ", handle: " + info_.name_ + " — empty socket object";
		error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}

	// send new message if any
	if (message_cache_->new_messages_count() == 0) {
		return false;
	}

	try {
		boost::shared_ptr<message_iface> new_msg = message_cache_->get_new_message();

		// send ident
		std::string ident = "elisto20f.dev.yandex.net/rimz_app@1/rimz_func";
		zmq::message_t ident_chunk(ident.size());
		memcpy((void *)ident_chunk.data(), ident.data(), ident.size());
		if (true != main_socket->send(ident_chunk, ZMQ_SNDMORE)) {
			++statistics_.bad_sent_messages;
			return false;
		}

		// send header
		zmq::message_t empty_chunk(0);
		if (true != main_socket->send(empty_chunk, ZMQ_SNDMORE)) {
			++statistics_.bad_sent_messages;
			return false;
		}

		msgpack::sbuffer sbuf;

		// send message uuid
		const std::string& uuid = new_msg->uuid();
		msgpack::pack(sbuf, uuid);

		zmq::message_t uuid_chunk(sbuf.size());
		memcpy((void *)uuid_chunk.data(), sbuf.data(), sbuf.size());

		if (true != main_socket->send(uuid_chunk, ZMQ_SNDMORE)) {
			++statistics_.bad_sent_messages;
			return false;
		}

		sbuf.clear();

		// send message policy
		policy_t server_policy = new_msg->policy().server_policy();

		// awful semantics! convert deadline [timeout value] to actual [deadline time]
		if (server_policy.deadline > 0.0) {
			time_value server_deadline = new_msg->enqued_timestamp();
			server_deadline += server_policy.deadline;
			server_policy.deadline = server_deadline.as_double();
		}

        msgpack::pack(sbuf, server_policy);

		zmq::message_t policy_chunk(sbuf.size());
		memcpy((void *)policy_chunk.data(), sbuf.data(), sbuf.size());

		if (true != main_socket->send(policy_chunk, ZMQ_SNDMORE)) {
			++statistics_.bad_sent_messages;
			return false;
		}

		// send data
		size_t data_size = new_msg->size();
		zmq::message_t data_chunk(data_size);

		if (data_size > 0) {
			new_msg->load_data();
			memcpy((void *)data_chunk.data(), new_msg->data(), data_size);
			new_msg->unload_data();
		}

		if (true != main_socket->send(data_chunk)) {
			++statistics_.bad_sent_messages;
			return false;
		}

		// assign message flags
		new_msg->mark_as_sent(true);

		// move message to sent
		message_cache_->move_new_message_to_sent();

		logger()->log(PLOG_DEBUG, "sent message with uuid: " + new_msg->uuid() +
					  " to [" + info_.name_ + "." + info_.service_name_ + "]");
	}
	catch (const std::exception& ex) {
		std::string error_msg = " service: " + info_.service_name_;
		error_msg += ", handle: " + info_.name_ + " — could not send message";
		error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION) + "reason: ";
		error_msg += ex.what();
		throw internal_error(error_msg);
	}

	return true;
}

bool
handle_t::receive_responce_chunk(socket_ptr_t& socket, zmq::message_t& response) {
	try {
		if (socket->recv(&response, ZMQ_NOBLOCK) == EAGAIN) {
			return false;
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "service: " + info_.service_name_;
		error_msg += ", handle: " + info_.name_ + " — error while receiving response chunk ";
		error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION) + "reason: ";
		error_msg += ex.what();
		logger()->log(PLOG_DEBUG, error_msg);

		return false;
	}

	return true;
}

bool
handle_t::dispatch_responce(socket_ptr_t& main_socket) {
	boost::mutex::scoped_lock lock(mutex_);

	boost::ptr_vector<zmq::message_t> response_chunks;

	// receive message
	int64_t more = 1;
	size_t more_size = sizeof(more);

	while (more) {
		zmq::message_t* chunk = new zmq::message_t;

		if (!receive_responce_chunk(main_socket, *chunk)) {
			delete chunk;
			break;
		}

		response_chunks.push_back(chunk);

		int rc = zmq_getsockopt(*main_socket, ZMQ_RCVMORE, &more, &more_size);
    	assert (rc == 0);
	}

	if (response_chunks.size() == 0) {
		return false;
	}

	lock.unlock();
	process_responce(response_chunks);
	return true;
}

void
handle_t::reshedule_message(const std::string& uuid) {
	// 2DO: must reshedule if allowed by policy
	message_cache_->move_sent_message_to_new_front(uuid);
}

void
handle_t::process_responce(boost::ptr_vector<zmq::message_t>& chunks) {
	boost::mutex::scoped_lock lock(mutex_);

	// unpack node identity
	std::string ident(reinterpret_cast<const char*>(chunks[0].data()));

	// unpack uuid
	std::string uuid;
	msgpack::unpacked msg;
	msgpack::unpack(&msg, reinterpret_cast<const char*>(chunks[2].data()), chunks[2].size());
	msgpack::object obj = msg.get();
    obj.convert(&uuid);

   	// unpack rpc code
	int rpc_code;
	msgpack::unpack(&msg, reinterpret_cast<const char*>(chunks[3].data()), chunks[3].size());
	obj = msg.get();
    obj.convert(&rpc_code);

	// get message from sent cache
	boost::shared_ptr<message_iface> sent_msg;
	try {
		sent_msg = message_cache_->get_sent_message(uuid);
	}
	catch (...) {
		// drop responce for missing message
		return;
	}

	std::string handle_desc = "[" + info_.name_ + "." + info_.service_name_ + "]";
	std::string message_str = "received response for message with uuid: " + sent_msg->uuid(); 
	message_str += " from " + handle_desc + ", type: ";

	std::string rpc_message_type_as_string;
	switch (rpc_code) {
		case SERVER_RPC_MESSAGE_CHUNK: 
			rpc_message_type_as_string = "CHUNK";
			break;

		case SERVER_RPC_MESSAGE_CHOKE: 
			rpc_message_type_as_string = "CHOKE";
			break;

		case SERVER_RPC_MESSAGE_ERROR: 
			rpc_message_type_as_string = "ERROR";
			break;
	}

	logger()->log(PLOG_DEBUG, message_str + rpc_message_type_as_string);

	switch (rpc_code) {
		case SERVER_RPC_MESSAGE_CHUNK: {
			// enqueue chunk in response queue
			cached_response_prt_t new_response;
			new_response.reset(new cached_response(uuid, sent_msg->path(), chunks[4].data(), chunks[4].size()));
			new_response->set_code(response_code::message_chunk);

			lock.unlock();
			enqueue_response(new_response);
			lock.lock();
		}
		break;

		case SERVER_RPC_MESSAGE_ERROR: {
			int error_code = -1;
			std::string error_message;

			// unpack error code
			msgpack::unpack(&msg, reinterpret_cast<const char*>(chunks[4].data()), chunks[4].size());
			obj = msg.get();
		    obj.convert(&error_code);

			// unpack error message
			msgpack::unpack(&msg, reinterpret_cast<const char*>(chunks[5].data()), chunks[5].size());
			obj = msg.get();
		    obj.convert(&error_message);

			if (error_code == resource_error) { // queue is full
				reshedule_message(uuid);

				std::string message_str = "resheduled message with uuid: " + uuid; 
				message_str += " from " + handle_desc + ", type: " + rpc_message_type_as_string;
				message_str += ", error code: ";

				logger()->log(PLOG_DEBUG, "%s%d, message: %s", message_str.c_str(), error_code, error_message.c_str());
			}
			else {
				cached_response_prt_t new_response;
				new_response.reset(new cached_response(uuid, sent_msg->path(), error_code, error_message));

				lock.unlock();
				enqueue_response(new_response);
				lock.lock();

				message_cache_->remove_message_from_cache(uuid);

				std::string message_str = "enqueued response for message with uuid: " + uuid; 
				message_str += " from " + handle_desc + ", type: " + rpc_message_type_as_string;
				message_str += ", error code: ";

				logger()->log(PLOG_DEBUG, "%s%d", message_str.c_str(), error_code);
			}
		}
		break;

		case SERVER_RPC_MESSAGE_CHOKE: {
			message_cache_->remove_message_from_cache(uuid);

			cached_response_prt_t new_response;
			new_response.reset(new cached_response(uuid, sent_msg->path(), NULL, 0));
			new_response->set_code(response_code::message_choke);

			lock.unlock();
			enqueue_response(new_response);
			lock.lock();
		}
		break;

		default:
			return;
	}
}

handle_stats&
handle_t::get_statistics() {

	handle_stats tmp_stats;
	if (context()->stats()->get_handle_stats(info_.service_name_,
											 info_.name_,
											 tmp_stats))
	{
		statistics_ = tmp_stats;
	}

	return statistics_;
}

void
handle_t::update_statistics() {
	statistics_.queue_status.pending = message_cache_->new_messages_count();
	statistics_.queue_status.sent = message_cache_->sent_messages_count();

	context()->stats()->update_handle_stats(info_.service_name_,
											info_.name_,
											statistics_);
}

bool
handle_t::check_for_responses(socket_ptr_t& main_socket, int poll_timeout) {
	if (!is_running_) {
		return false;
	}

	// validate socket
	assert(main_socket);

	// poll for responce
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *main_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	int socket_response = zmq_poll(poll_items, 1, poll_timeout);

	if (socket_response <= 0) {
		return false;
	}

	// in case we received message response
	if ((ZMQ_POLLIN & poll_items[0].revents) == ZMQ_POLLIN) {
		return true;
	}

    return false;
}

void
handle_t::log_dispatch_start() {
	static bool started = false;

	if (!started) {
		std::string format = "thread started for service: %s, handle: %s";
		logger()->log(PLOG_DEBUG, format.c_str(), info_.service_name_.c_str(), info_.name_.c_str());
		started = true;
	}
}

const handle_info_t&
handle_t::info() const {
	return info_;
}

void
handle_t::kill() {
	if (!is_running_) {
		return;
	}

	// kill dispatch thread from the inside
	int control_message = CONTROL_MESSAGE_KILL;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

std::string
handle_t::description() {
	return std::string("[" + info_.service_name_ +"].[" + info_.name_ + "]");
}

void
handle_t::log_connection(const std::string prefix, const endpoints_list_t& endpoints) {
	std::string log_str = prefix + " " + description();

	if (prefix == "DISCONNECT HANDLE") {
		logger()->log(PLOG_DEBUG, log_str + " from all endpoints.");
		return;
	}

	log_str += " to endpoints: ";
	
	for (size_t i = 0; i < endpoints.size(); ++i) {
		log_str += endpoints[i].endpoint_;

		if (i != endpoints.size() - 1) {
			log_str += ", ";
		}
	}

	logger()->log(PLOG_DEBUG, log_str);
}

void
handle_t::connect() {
	boost::mutex::scoped_lock lock(mutex_);

	if (!is_running_ || endpoints_.empty() || is_connected_) {
		return;
	}

	log_connection("CONNECT HANDLE", endpoints_);

	// connect to hosts
	int control_message = CONTROL_MESSAGE_CONNECT;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

/*
void
handle_t::connect(const hosts_info_list_t& hosts) {
	boost::mutex::scoped_lock lock(mutex_);

	// no hosts to connect to
	if (!is_running_ || is_connected_ || hosts.empty()) {
		return;
	}
	else {
		// store new hosts
		hosts_ = hosts;
	}

	log_connection("CONNECT HANDLE", hosts);

	// connect to hosts
	lock.unlock();
	connect();
}

void
handle_t::connect_new_hosts(const hosts_info_list_t& hosts) {
	boost::mutex::scoped_lock lock(mutex_);

	// no new hosts to connect to
	if (!is_running_ || hosts.empty()) {
		return;
	}
	else {
		// append new hosts
		new_hosts_.insert(new_hosts_.end(), hosts.begin(), hosts.end());
	}

	log_connection("CONNECT NEW HOSTS TO HANDLE", hosts);

	// connect to new hosts
	int control_message = CONTROL_MESSAGE_CONNECT_NEW_HOSTS;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

void
handle_t::reconnect(const hosts_info_list_t& hosts) {
	boost::mutex::scoped_lock lock(mutex_);

	// no new hosts to connect to
	if (!is_running_ || hosts.empty()) {
		return;
	}
	else {
		// replace hosts with new hosts
		hosts_ = hosts;
	}

	log_connection("RECONNECT HANDLE", hosts);

	// reconnect to hosts
	int control_message = CONTROL_MESSAGE_RECONNECT;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}
*/

void
handle_t::disconnect() {
	boost::mutex::scoped_lock lock(mutex_);

	if (!is_running_) {
		return;
	}

	log_connection("DISCONNECT HANDLE", endpoints_);

	// disconnect from all hosts
	std::string control_message = boost::lexical_cast<std::string>(CONTROL_MESSAGE_DISCONNECT);
	zmq::message_t message(control_message.length());
	memcpy((void *)message.data(), control_message.c_str(), control_message.length());
	zmq_control_socket_->send(message);
}

void
handle_t::make_all_messages_new() {
	assert (message_cache_);
	return message_cache_->make_all_messages_new();
}

message_cache::message_queue_ptr_t
handle_t::new_messages() {
	assert (message_cache_);
	return message_cache_->new_messages();
}

void
handle_t::assign_message_queue(const message_cache::message_queue_ptr_t& message_queue) {
	assert (message_cache_);
	return message_cache_->append_message_queue(message_queue);
}

void
handle_t::set_responce_callback(responce_callback_t callback) {
	boost::mutex::scoped_lock lock(mutex_);
	response_callback_ = callback;
}

void
handle_t::enqueue_message(const boost::shared_ptr<message_iface>& message) {
	boost::mutex::scoped_lock lock(mutex_);

	message_cache_->enqueue(message);
	update_statistics();
}

boost::shared_ptr<cocaine::dealer::context>
handle_t::context() {
	assert(context_);
	return context_;
}

boost::shared_ptr<base_logger>
handle_t::logger() {
	return context()->logger();
}

boost::shared_ptr<configuration>
handle_t::config() {
	boost::shared_ptr<configuration> conf = context()->config();
	assert (conf);
	return conf;
}

} // namespace dealer
} // namespace cocaine
