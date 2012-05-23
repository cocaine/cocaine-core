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
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/uuid.hpp"

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

	logger()->log(PLOG_DEBUG, "CREATED HANDLE " + description());

	// create message cache
	message_cache_.reset(new message_cache(context(), config()->message_cache_type()));

	// create control socket
	std::string conn_str = "inproc://service_control_" + description();
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

	logger()->log(PLOG_DEBUG, "KILLED HANDLE " + description());
}

void
handle_t::dispatch_messages() {	
	std::string balancer_ident = info_.as_string() + "." + wuuid_t().generate();
	balancer_t balancer(balancer_ident, endpoints_, context(), message_cache_);

	socket_ptr_t control_socket;
	establish_control_conection(control_socket);

	logger()->log(PLOG_DEBUG, "started message dispatch for " + description());

	last_response_timer_.reset();
	deadlined_messages_timer_.reset();

	// process messages
	while (is_running_) {
		boost::mutex::scoped_lock lock(mutex_);

		// process incoming control messages
		int control_message = receive_control_messages(control_socket, 0);
		if (control_message == CONTROL_MESSAGE_KILL) {
			// stop message dispatch, finalize everything
			is_running_ = false;
			break;
		}
		else if (control_message > 0) {
			dispatch_control_messages(control_message, balancer);
		}

		// send new message if any
		if (is_running_ && is_connected_) {
			for (int i = 0; i < 100; ++i) { // batching
				if (message_cache_->new_messages_count() == 0) {
					break;	
				}

				dispatch_next_available_message(balancer);
			}
		}

		// check for message responces
		bool received_response = false;

		int fast_poll_timeout = 10;		// microsecs
		int long_poll_timeout = 1000;	// microsecs

		int response_poll_timeout = fast_poll_timeout;
		if (last_response_timer_.elapsed().as_double() > 0.5f) {	// 0.5f == 500 millisecs
			response_poll_timeout = long_poll_timeout;			
		}

		if (is_connected_ && is_running_) {
			received_response = balancer.check_for_responses(response_poll_timeout);

			// process received responce(s)
			while (received_response) {
				last_response_timer_.reset();
				response_poll_timeout = fast_poll_timeout;

				lock.unlock();
				dispatch_next_available_response(balancer);
				lock.lock();

				received_response = balancer.check_for_responses(response_poll_timeout);
			}
		}

		if (is_running_) {
			if (deadlined_messages_timer_.elapsed().as_double() > 0.01f) {	// 0.01f == 10 millisecs
				lock.unlock();
				process_deadlined_messages();
				lock.lock();
				deadlined_messages_timer_.reset();
			}
		}
	}

	control_socket.reset();

	update_statistics();
	logger()->log(PLOG_DEBUG, "finished message dispatch for " + description());
}

void
handle_t::dispatch_next_available_response(balancer_t& balancer) {
	cached_response_prt_t response;
	if (!balancer.receive(response)) {
		return;
	}

	switch (response->code()) {
		case response_code::message_chunk:
			enqueue_response(response);
		break;

		case response_code::message_choke:
			message_cache_->remove_message_from_cache(response->uuid());
			enqueue_response(response);
		break;

		case resource_error: {
			if (reshedule_message(response->uuid())) {
				std::string message_str = "resheduled msg with uuid: " + response->uuid();
				message_str += " from " + description() + ", type: ERROR";
				message_str += ", error code: ";

				logger()->log(PLOG_DEBUG,
							  "%s%d, error message: %s",
							  message_str.c_str(),
							  response->code(),
							  response->error_message().c_str());
			}
			else {
				enqueue_response(response);
				message_cache_->remove_message_from_cache(response->uuid());
			}
		}
		break;

		default: {
			enqueue_response(response);
			message_cache_->remove_message_from_cache(response->uuid());

			std::string message_str = "enqued response for msg with uuid: " + response->uuid();
			message_str += " from " + description() + ", type: ERROR";
			message_str += ", error code: ";

			logger()->log(PLOG_DEBUG,
						  "%s%d, error message: %s",
						  message_str.c_str(),
						  response->code(),
						  response->error_message().c_str());
		}
		break;
	}
}

void
handle_t::dispatch_control_messages(int type, balancer_t& balancer) {
	if (!is_running_) {
		return;
	}

	switch (type) {
		case CONTROL_MESSAGE_CONNECT:
			if (!is_connected_) {
				balancer.connect(endpoints_);
				is_connected_ = true;
			}
			break;

		case CONTROL_MESSAGE_UPDATE:
			if (is_connected_) {
				std::vector<cocaine_endpoint> missing_endpoints;
				balancer.update_endpoints(endpoints_, missing_endpoints);
			}
			break;

		case CONTROL_MESSAGE_DISCONNECT:
			balancer.disconnect();
			is_connected_ = false;
			break;
	}
}

boost::shared_ptr<message_cache>
handle_t::messages_cache() const {
	return message_cache_;
}

void
handle_t::process_deadlined_messages() {
	assert(message_cache_);
	message_cache::message_queue_t expired_messages;
	message_cache_->get_expired_messages(expired_messages);

	if (expired_messages.empty()) {
		return;
	}

	for (size_t i = 0; i < expired_messages.size(); ++i) {
		if (!expired_messages.at(i)->ack_received()) {
			if (expired_messages.at(i)->can_retry()) {
				expired_messages.at(i)->increment_retries_count();
				message_cache_->enqueue_with_priority(expired_messages.at(i));

				logger()->log(PLOG_DEBUG, "no ACK, resheduled message " + expired_messages.at(i)->uuid());
			}
			else {
				logger()->log(PLOG_DEBUG, "reshedule message policy exceeded, did not receive ACK " + expired_messages.at(i)->uuid());
				cached_response_prt_t new_response;
				new_response.reset(new cached_response_t(expired_messages.at(i)->uuid(),
														 expired_messages.at(i)->path(),
														 request_error,
														 "server did not reply with ack in time"));
				enqueue_response(new_response);
			}
		}
		else {
			cached_response_prt_t new_response;
			new_response.reset(new cached_response_t(expired_messages.at(i)->uuid(),
												   expired_messages.at(i)->path(),
												   deadline_error,
												   "message expired 2"));
			enqueue_response(new_response);
		}
	}
}

void
handle_t::establish_control_conection(socket_ptr_t& control_socket) {
	control_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_PAIR));

	if (control_socket.get()) {
		std::string conn_str = "inproc://service_control_" + description();

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
		std::string error_msg = "some very ugly shit happend while recv on control socket at ";
		error_msg += std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " details: " + std::string(ex.what());
		throw internal_error(error_msg);
	}

    if (recv_failed) {
    	std::string sname = info_.service_name_;
    	std::string hname = info_.name_;
    	logger()->log("control socket recv failed on " + description());
    }

    return 0;
}

bool
handle_t::dispatch_next_available_message(balancer_t& balancer) {
	// send new message if any
	if (message_cache_->new_messages_count() == 0) {
		return false;
	}

	boost::shared_ptr<message_iface> new_msg = message_cache_->get_new_message();
	
	if (balancer.send(new_msg)) {
		new_msg->mark_as_sent(true);
		message_cache_->move_new_message_to_sent();

		std::string log_msg = "sent msg with uuid: %s to %s";
		logger()->log(PLOG_DEBUG, log_msg.c_str(), new_msg->uuid().c_str(), description().c_str());

		return true;
	}
	else {
		logger()->log("PPZ");		
	}

	return false;
}

bool
handle_t::reshedule_message(const std::string& uuid) {
	boost::shared_ptr<message_iface> msg = message_cache_->get_sent_message(uuid);

	if (msg->can_retry()) {
		msg->increment_retries_count();
		message_cache_->move_sent_message_to_new_front(msg->uuid());
		return true;
	}

	return false;
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
	return info_.as_string();
}

void
handle_t::connect() {
	boost::mutex::scoped_lock lock(mutex_);

	if (!is_running_ || endpoints_.empty() || is_connected_) {
		return;
	}

	logger()->log(PLOG_DEBUG, "CONNECT HANDLE " + description());

	// connect to hosts
	int control_message = CONTROL_MESSAGE_CONNECT;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

void
handle_t::update_endpoints(const std::vector<cocaine_endpoint>& endpoints) {
	boost::mutex::scoped_lock lock(mutex_);

	if (!is_running_ || endpoints.empty()) {
		return;
	}

	endpoints_ = endpoints;
	logger()->log(PLOG_DEBUG, "UPDATE HANDLE " + description());

	// connect to hosts
	int control_message = CONTROL_MESSAGE_UPDATE;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_->send(message);
}

void
handle_t::disconnect() {
	boost::mutex::scoped_lock lock(mutex_);

	if (!is_running_) {
		return;
	}

	logger()->log(PLOG_DEBUG, "DISCONNECT HANDLE " + description());

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
