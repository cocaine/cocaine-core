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

#include "json/json.h"

#include "cocaine/dealer/structs.hpp"

#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/core/handle_info.hpp"
#include "cocaine/dealer/core/host_info.hpp"
#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/core/cached_response.hpp"
#include "cocaine/dealer/core/message_cache.hpp"
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

// predeclaration
template <typename LSD_T> class handle;
typedef handle<DT> handle_t;

template <typename LSD_T>
class handle : public boost::noncopyable {
public:
	typedef std::vector<host_info<LSD_T> > hosts_info_list_t;
	typedef boost::shared_ptr<zmq::socket_t> socket_ptr_t;

	typedef boost::shared_ptr<cached_response> cached_response_prt_t;
	typedef boost::function<void(cached_response_prt_t)> responce_callback_t;

public:
	handle(const handle_info<LSD_T>& info,
		   boost::shared_ptr<cocaine::dealer::context> context,
		   const hosts_info_list_t& hosts);

	virtual ~handle();

	const handle_info<LSD_T>& info() const;
	boost::shared_ptr<message_cache> messages_cache();

	void connect();
	void connect(const hosts_info_list_t& hosts);
	void connect_new_hosts(const hosts_info_list_t& hosts);
	void reconnect(const hosts_info_list_t& hosts);
	void disconnect();

	void set_responce_callback(responce_callback_t callback);
	void enqueue_message(boost::shared_ptr<message_iface> message);

private:
	void kill();
	void dispatch_messages();
	void log_dispatch_start();

	// working with control messages
	void establish_control_conection(socket_ptr_t& control_socket);
	int receive_control_messages(socket_ptr_t& control_socket);
	void dispatch_control_messages(int type, socket_ptr_t& main_socket);

	// working with messages
	bool dispatch_next_available_message(socket_ptr_t main_socket);
	void connect_zmq_socket_to_hosts(socket_ptr_t& socket,
									 hosts_info_list_t& hosts);

	bool check_for_responses(socket_ptr_t& main_socket);
	void dispatch_responces(socket_ptr_t& main_socket);

	void enqueue_response(cached_response_prt_t response);

	// send collected statistics to global stats collector
	handle_stats& get_statistics();
	void update_statistics();

	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config();
	boost::shared_ptr<cocaine::dealer::context> context();

private:
	handle_info<LSD_T> info_;
	boost::shared_ptr<cocaine::dealer::context> context_;
	hosts_info_list_t hosts_;
	hosts_info_list_t new_hosts_;
	boost::shared_ptr<message_cache> message_cache_;

	boost::thread thread_;
	boost::mutex mutex_;
	volatile bool is_running_;
	volatile bool is_connected_;

	std::auto_ptr<zmq::socket_t> zmq_control_socket_;
	bool receiving_control_socket_ok_;

	responce_callback_t response_callback_;

	handle_stats statistics_;
};

template <typename LSD_T>
handle<LSD_T>::handle(const handle_info<LSD_T>& info,
					  boost::shared_ptr<cocaine::dealer::context> lsd_context,
					  const hosts_info_list_t& hosts) :
	info_(info),
	context_(lsd_context),
	hosts_(hosts),
	is_running_(false),
	is_connected_(false),
	receiving_control_socket_ok_(false)
{
	logger()->log(PLOG_DEBUG, "STARTED HANDLE [%s].[%s]", info.service_name_.c_str(), info.name_.c_str());

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
	thread_ = boost::thread(&handle<LSD_T>::dispatch_messages, this);

	get_statistics();
	update_statistics();
}

template <typename LSD_T>
handle<LSD_T>::~handle() {
	kill();

	zmq_control_socket_->close();
	zmq_control_socket_.reset(NULL);

	thread_.join();
}

template <typename LSD_T> void
handle<LSD_T>::dispatch_messages() {
	// establish connections
	socket_ptr_t main_socket;
	socket_ptr_t control_socket;

	establish_control_conection(control_socket);

	std::string log_str = "started message dispatch for [%s].[%s]";
	logger()->log(PLOG_DEBUG, log_str.c_str(), info_.service_name_.c_str(), info_.name_.c_str());

	// process messages
	while (is_running_) {

		// receive control message
		int control_message = receive_control_messages(control_socket);

		// received kill message, finalize everything
		if (control_message == CONTROL_MESSAGE_KILL) {
			is_running_ = false;
			break;
		}

		// process incoming control messages
		if (control_message > 0) {
			dispatch_control_messages(control_message, main_socket);
		}

		// send new message if any
		if (is_running_ && is_connected_) {
			if (dispatch_next_available_message(main_socket)) {
				++statistics_.sent_messages;
			}
		}

		// check for message responces
		bool received_response = false;
		if (is_connected_ && is_running_) {
			received_response = check_for_responses(main_socket);
		}

		// process received responce(s)
		if (is_connected_ && is_running_ && received_response) {
			dispatch_responces(main_socket);
		}

		/*
		// check for expired messages
		if (is_connected_ && is_running_) {
			std::vector<std::pair<std::string, message_path> > expired;
			messages_cache()->process_expired_messages(expired);

			// put expired messages to response queue
			for (size_t i = 0; i < expired.size(); ++i) {
				// shortcuts
				std::string& uuid = expired[i].first;
				message_path& path = expired[i].second;
				std::string error_msg = "the job has expired";

				// create response
				cached_response_prt_t response;
				response.reset(new cached_response(uuid, path, EXPIRED_MESSAGE_ERROR, error_msg));
				enqueue_response(response);

				++statistics_.expired_responses;
			}
		}
		*/

		update_statistics();
	}

	control_socket.reset();
	main_socket.reset();

	update_statistics();
	log_str = "finished message dispatch for [%s].[%s]";
	logger()->log(PLOG_DEBUG, log_str.c_str(), info_.service_name_.c_str(), info_.name_.c_str());
}

template <typename LSD_T> void
handle<LSD_T>::establish_control_conection(socket_ptr_t& control_socket) {
	control_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_PAIR));

	if (control_socket.get()) {
		std::string conn_str = "inproc://service_control_" + info_.service_name_ + "_" + info_.name_;

		int timeout = 0;
		control_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
		control_socket->connect(conn_str.c_str());
		receiving_control_socket_ok_ = true;
	}
}

template <typename LSD_T> void
handle<LSD_T>::enqueue_response(cached_response_prt_t response) {
	if (response_callback_) {
		response_callback_(response);
	}
}

template <typename LSD_T> int
handle<LSD_T>::receive_control_messages(socket_ptr_t& control_socket) {
	if (!is_running_) {
		return 0;
	}

	// poll for responce
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *control_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	int socket_response = zmq_poll(poll_items, 1, 0);

	if (socket_response <= 0) {
		return 0;
	}

	// in case we received control message
    if ((ZMQ_POLLIN & poll_items[0].revents) == ZMQ_POLLIN) {
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
			throw error(error_msg);
    	}

        if (recv_failed) {
        	std::string sname = info_.service_name_;
        	std::string hname = info_.name_;
        	logger()->log("recv failed on service: %s, handle %s", sname.c_str(), hname.c_str());
        }
    }

    return 0;
}

template <typename LSD_T> void
handle<LSD_T>::dispatch_control_messages(int type, socket_ptr_t& main_socket) {
	if (!is_running_) {
		return;
	}

	switch (type) {
		case CONTROL_MESSAGE_CONNECT:
			//logger()->log(PLOG_DEBUG, "CONTROL_MESSAGE_CONNECT");

			// create new main socket in case we're not connected
			if (!is_connected_) {
				main_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_DEALER));
				connect_zmq_socket_to_hosts(main_socket, hosts_);
				is_connected_ = true;
			}
			break;

		case CONTROL_MESSAGE_RECONNECT:
			//logger()->log(PLOG_DEBUG, "CONTROL_MESSAGE_RECONNECT");

			// kill old socket, create new
			main_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_DEALER));
			connect_zmq_socket_to_hosts(main_socket, hosts_);
			is_connected_ = true;
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
					connect_zmq_socket_to_hosts(main_socket, new_hosts);
				}
			}
			break;
	}
}

template <typename LSD_T> void
handle<LSD_T>::connect_zmq_socket_to_hosts(socket_ptr_t& socket,
										   hosts_info_list_t& hosts)
{
	if (hosts.empty()) {
		return;
	}

	if (!socket.get()) {
		std::string error_msg = "service: " + info_.service_name_;
		error_msg += ", handle: " + info_.name_ + " — socket object is empty";
		error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_msg);
	}

	// connect socket to hosts
	std::string connection_str;
	try {
		for (size_t i = 0; i < hosts.size(); ++i) {
			std::string port = boost::lexical_cast<std::string>(info_.port_);
			std::string ip = host_info<LSD_T>::string_from_ip(hosts[i].ip_);
			connection_str = "tcp://" + ip + ":" + port;
			//logger()->log(PLOG_DEBUG, "handle connection str: %s", connection_str.c_str());
			
			int timeout = 0;
			socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));

			int64_t hwm = 10000;
			socket->setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));

			socket->connect(connection_str.c_str());
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "service: " + info_.service_name_;
		error_msg += ", handle: " + info_.name_ + " — could not connect to ";
		error_msg += connection_str + " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_msg);
	}
}

template <typename LSD_T> bool
handle<LSD_T>::dispatch_next_available_message(socket_ptr_t main_socket) {
	// validate socket
	if (!main_socket) {
		std::string error_msg = "service: " + info_.service_name_;
		error_msg += ", handle: " + info_.name_ + " — empty socket object";
		error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_msg);
	}

	// send new message if any
	if (messages_cache()->new_messages_count() == 0) {
		return false;
	}

	try {
		boost::shared_ptr<message_iface> new_msg = messages_cache()->get_new_message();

		// send header
		zmq::message_t empty_message(0);
		if (true != main_socket->send(empty_message, ZMQ_SNDMORE)) {
			++statistics_.bad_sent_messages;
			return false;
		}

		// send envelope
		std::string msg_header = new_msg->json();
		size_t header_size = msg_header.length();
		zmq::message_t header(header_size);
		memcpy((void *)header.data(), msg_header.c_str(), header_size);

		if (true != main_socket->send(header, ZMQ_SNDMORE)) {
			++statistics_.bad_sent_messages;
			return false;
		}

		// send data
		size_t data_size = new_msg->size();
		zmq::message_t message(data_size);

		if (data_size > 0) {
			new_msg->load_data();
			memcpy((void *)message.data(), new_msg->data(), data_size);
			new_msg->unload_data();
		}

		if (true != main_socket->send(message)) {
			++statistics_.bad_sent_messages;
			return false;
		}

		// assign message flags
		new_msg->mark_as_sent(true);

		// move message to sent
		messages_cache()->move_new_message_to_sent();
	}
	catch (const std::exception& ex) {
		std::string error_msg = " service: " + info_.service_name_;
		error_msg += ", handle: " + info_.name_ + " — could not send message";
		error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION) + "reason: ";
		error_msg += ex.what();
		throw error(error_msg);
	}

	return true;
}

template <typename LSD_T> void
handle<LSD_T>::dispatch_responces(socket_ptr_t& main_socket) {
	zmq::message_t reply;

	// receive messages while we have any on the socket
	while (true) {
		std::string json_header;

		try {
			// receive reply header
			if (!main_socket->recv(&reply, ZMQ_NOBLOCK)) {
				break;
			}

			// receive json envelope
			if (!main_socket->recv(&reply)) {
				logger()->log(PLOG_ERROR, "bad envelope");
				break;
			}
		}
		catch (const std::exception& ex) {
			std::string error_msg = "service: " + info_.service_name_;
			error_msg += ", handle: " + info_.name_ + " — error while receiving response envelope ";
			error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION) + "reason: ";
			error_msg += ex.what();
			logger()->log(PLOG_DEBUG, error_msg);
			break;
		}

		bool is_response_completed = false;
		std::string uuid;
		Json::Value envelope_val;
		Json::Reader jreader;
		int error_code = 0;
		std::string error_message;

		try {
			// get envelope json string
			json_header = std::string((const char*)reply.data(), reply.size());
			//logger()->log(PLOG_DEBUG, "received header: %s", json_header.c_str());

			// parse envelope json
			if (jreader.parse(json_header.c_str(), envelope_val)) {
				uuid = envelope_val.get("uuid", "").asString();
				is_response_completed = envelope_val.get("completed", false).asBool();
				error_code = envelope_val.get("code", 0).asInt();
				error_message = envelope_val.get("message", "").asString();

				if (uuid.empty()) {
					std::string error_msg = "service: " + info_.service_name_;
					error_msg += ", handle: " + info_.name_ + " — uuid is empty in response envelope";
					error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
					throw error(error_msg);
				}
			}
			else {
				std::string error_msg = "service: " + info_.service_name_;
				error_msg += ", handle: " + info_.name_ + " — could not parse response envelope";
				error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
				throw error(error_msg);
			}
		}
		catch (const std::exception& ex) {
			std::string error_msg = "service: " + info_.service_name_;
			error_msg += ", handle: " + info_.name_ + " — error while parsing response envelope";
			error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION) + "reason: ";
			error_msg += ex.what();
			logger()->log(PLOG_DEBUG, error_msg);
			break;
		}
		
		// get message from sent cache
		bool fetched_message = false;
		boost::shared_ptr<message_iface> sent_msg;

		try {
			try {
				sent_msg = messages_cache()->get_sent_message(uuid);
				fetched_message = true;
			}
			catch (...) {
			}

			// just send message again
			if (error_code == MESSAGE_QUEUE_IS_FULL) {
				if (fetched_message) {
					//messages_cache()->remove_message_from_cache(uuid);
					messages_cache()->move_sent_message_to_new_front(uuid);
					++statistics_.resent_messages;
					update_statistics();
					continue;
				}
			}
		}
		catch (const std::exception& ex) {
			std::string error_msg = "service: " + info_.service_name_;
			error_msg += ", handle: " + info_.name_ + " — error while retrieving response message";
			error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION) + "reason: ";
			error_msg += ex.what();
			logger()->log(PLOG_DEBUG, error_msg);
			break;
		}

		try {
			if (error_code != 0) {
				logger()->log(PLOG_DEBUG, "error code: %d, message: %s", error_code, error_message.c_str());

				// if we could not get message from cache, we assume, dealer has already processed it
				// otherwise — make response!
				if (fetched_message) {
					// create response object
					cached_response_prt_t new_response;
					new_response.reset(new cached_response(uuid, sent_msg->path(), error_code, error_message));

					// remove message from cache
					messages_cache()->remove_message_from_cache(uuid);
					enqueue_response(new_response);

					// statistics
					++statistics_.err_responces;

					if (error_code == EXPIRED_MESSAGE_ERROR) {
						++statistics_.expired_responses;
					}
					else {
						++statistics_.err_responces;
					}

					++statistics_.all_responces;
					update_statistics();
				}

				continue;
			}
		}
		catch (const std::exception& ex) {
			std::string error_msg = "service: " + info_.service_name_;
			error_msg += ", handle: " + info_.name_ + " — error while resending message";
			error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION) + "reason: ";
			error_msg += ex.what();
			logger()->log(PLOG_DEBUG, error_msg);
			break;
		}

		try {
			// receive data
			if (!is_response_completed) {
				//logger()->log(PLOG_DEBUG, "responce not completed");

				// receive data chunk
				if (!main_socket->recv(&reply)) {
					logger()->log(PLOG_DEBUG, "bad data");
					break;
				}

				msgpack::unpacked msg;
				msgpack::unpack(&msg, (const char*)reply.data(), reply.size());

				msgpack::object obj = msg.get();
				std::stringstream stream;
				stream << obj;
				//logger()->log(PLOG_DEBUG, "received data: %s", stream.str().c_str());

				if (fetched_message) {
					++statistics_.normal_responces;
					++statistics_.all_responces;
					update_statistics();

					cached_response_prt_t new_response;
					new_response.reset(new cached_response(uuid, sent_msg->path(), reply.data(), reply.size()));
					new_response->set_error(MESSAGE_CHUNK, "");
					enqueue_response(new_response);
				}
			}
			else {
				//logger()->log(PLOG_DEBUG, "responce completed");
				messages_cache()->remove_message_from_cache(uuid);

				if (fetched_message) {
					++statistics_.normal_responces;
					++statistics_.all_responces;
					update_statistics();

					cached_response_prt_t new_response;
					new_response.reset(new cached_response(uuid, sent_msg->path(), NULL, 0));
					new_response->set_error(MESSAGE_CHOKE, "");
					enqueue_response(new_response);
				}
			}
		}
		catch (const std::exception& ex) {
			std::string error_msg = "service: " + info_.service_name_;
			error_msg += ", handle: " + info_.name_ + " — error while receiving response data";
			error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION) + "reason: ";
			error_msg += ex.what();
			logger()->log(PLOG_DEBUG, error_msg);
			break;
		}
	}
}

template <typename LSD_T> handle_stats&
handle<LSD_T>::get_statistics() {

	handle_stats tmp_stats;
	if (context()->stats()->get_handle_stats(info_.service_name_,
											 info_.name_,
											 tmp_stats))
	{
		statistics_ = tmp_stats;
	}

	return statistics_;
}

template <typename LSD_T> void
handle<LSD_T>::update_statistics() {
	statistics_.queue_status.pending = messages_cache()->new_messages_count();
	statistics_.queue_status.sent = messages_cache()->sent_messages_count();

	context()->stats()->update_handle_stats(info_.service_name_,
											info_.name_,
											statistics_);
}

template <typename LSD_T> bool
handle<LSD_T>::check_for_responses(socket_ptr_t& main_socket) {
	if (!is_running_) {
		return false;
	}

	// validate socket
	if (!main_socket.get()) {
		std::string error_msg = "service: " + info_.service_name_;
		error_msg += ", handle: " + info_.name_ + " — empty socket object";
		error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_msg);
	}

	// poll for responce
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *main_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	int socket_response = zmq_poll(poll_items, 1, 0);

	if (socket_response <= 0) {
		return false;
	}

	// in case we received message response
	if ((ZMQ_POLLIN & poll_items[0].revents) == ZMQ_POLLIN) {
		return true;
	}

    return false;
}

template <typename LSD_T> void
handle<LSD_T>::log_dispatch_start() {
	static bool started = false;

	if (!started) {
		std::string format = "thread started for service: %s, handle: %s";
		logger()->log(PLOG_DEBUG, format.c_str(), info_.service_name_.c_str(), info_.name_.c_str());
		started = true;
	}
}

template <typename LSD_T> const handle_info<LSD_T>&
handle<LSD_T>::info() const {
	return info_;
}

template <typename LSD_T> void
handle<LSD_T>::kill() {
	logger()->log(PLOG_DEBUG, "killing handle [%s].[%s]", info_.service_name_.c_str(), info_.name_.c_str());

	if (!is_running_) {
		return;
	}

	// kill dispatch thread from the inside
	int control_message = CONTROL_MESSAGE_KILL;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

template <typename LSD_T> void
handle<LSD_T>::connect() {
	//logger()->log(PLOG_DEBUG, "connect");

	get_statistics();
	update_statistics();

	if (!is_running_ || hosts_.empty() || is_connected_) {
		return;
	}

	// connect to hosts
	int control_message = CONTROL_MESSAGE_CONNECT;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

template <typename LSD_T> void
handle<LSD_T>::connect(const hosts_info_list_t& hosts) {
	std::string log_str = "CONNECT HANDLE [" + info_.service_name_ +"].[" + info_.name_ + "] to hosts: ";
	for (size_t i = 0; i < hosts.size(); ++i) {
		log_str += host_info_t::string_from_ip(hosts[i].ip_);

		if (i != hosts.size() - 1) {
			log_str += ", ";
		}
	}

	logger()->log(PLOG_DEBUG, log_str);

	get_statistics();
	update_statistics();

	// no hosts to connect to
	if (!is_running_ || is_connected_ || hosts.empty()) {
		return;
	}
	else {
		// store new hosts
		boost::mutex::scoped_lock lock(mutex_);
		hosts_ = hosts;
	}

	// connect to hosts
	connect();
}

template <typename LSD_T> void
handle<LSD_T>::connect_new_hosts(const hosts_info_list_t& hosts) {
	std::string log_str = "CONNECT HANDLE [" + info_.service_name_ +"].[" + info_.name_ + "] to new hosts: ";
	for (size_t i = 0; i < hosts.size(); ++i) {
		log_str += host_info_t::string_from_ip(hosts[i].ip_);

		if (i != hosts.size() - 1) {
			log_str += ", ";
		}
	}

	logger()->log(PLOG_DEBUG, log_str);



	get_statistics();
	update_statistics();

	// no new hosts to connect to
	if (!is_running_ || hosts.empty()) {
		return;
	}
	else {
		// append new hosts
		boost::mutex::scoped_lock lock(mutex_);
		new_hosts_.insert(new_hosts_.end(), hosts.begin(), hosts.end());
	}

	// connect to new hosts
	int control_message = CONTROL_MESSAGE_CONNECT_NEW_HOSTS;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

template <typename LSD_T> void
handle<LSD_T>::reconnect(const hosts_info_list_t& hosts) {
	logger()->log(PLOG_DEBUG, "reconnect");

	get_statistics();
	update_statistics();

	// no new hosts to connect to
	if (!is_running_ || hosts.empty()) {
		return;
	}
	else {
		// replace hosts with new hosts
		boost::mutex::scoped_lock lock(mutex_);
		hosts_ = hosts;
	}

	// reconnect to hosts
	int control_message = CONTROL_MESSAGE_RECONNECT;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

template <typename LSD_T> void
handle<LSD_T>::disconnect() {
	boost::mutex::scoped_lock lock(mutex_);
	logger()->log(PLOG_DEBUG, "disconnect");

	if (!is_running_) {
		return;
	}

	// disconnect from all hosts
	std::string control_message = boost::lexical_cast<std::string>(CONTROL_MESSAGE_DISCONNECT);
	zmq::message_t message(control_message.length());
	memcpy((void *)message.data(), control_message.c_str(), control_message.length());
	zmq_control_socket_->send(message);
}

template <typename LSD_T> boost::shared_ptr<message_cache>
handle<LSD_T>::messages_cache() {
	if (!message_cache_) {
		std::string error_str = "messages cache object is empty for service: ";
		error_str += info_.service_name_ + ", handle: " + info_.name_;
		error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_str);
	}

	return message_cache_;
}

template <typename LSD_T> void
handle<LSD_T>::set_responce_callback(responce_callback_t callback) {
	boost::mutex::scoped_lock lock(mutex_);
	response_callback_ = callback;
}

template <typename LSD_T> void
handle<LSD_T>::enqueue_message(boost::shared_ptr<message_iface> message) {
	boost::mutex::scoped_lock lock(mutex_);
	messages_cache()->enqueue(message);

	update_statistics();
}

template <typename LSD_T> boost::shared_ptr<cocaine::dealer::context>
handle<LSD_T>::context() {
	if (!context_) {
		throw error("dealer context object is empty at " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return context_;
}

template <typename LSD_T> boost::shared_ptr<base_logger>
handle<LSD_T>::logger() {
	return context()->logger();
}

template <typename LSD_T> boost::shared_ptr<configuration>
handle<LSD_T>::config() {
	boost::shared_ptr<configuration> conf = context()->config();
	if (!conf.get()) {
		throw error("configuration object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return conf;
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HANDLE_HPP_INCLUDED_
