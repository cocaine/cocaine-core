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

#include "cocaine/dealer/core/handle.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/uuid.hpp"

namespace cocaine {
namespace dealer {

handle_t::handle_t(const handle_info_t& info,
				   const endpoints_list_t& endpoints,
				   const boost::shared_ptr<context_t>& ctx,
				   bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled),
	info_m(info),
	endpoints_m(endpoints),
	is_running_m(false),
	is_connected_m(false),
	receiving_control_socket_ok_m(false)
{
	boost::mutex::scoped_lock lock(mutex_);

	log(PLOG_DEBUG, "CREATED HANDLE " + description());

	// create message cache
	message_cache_m.reset(new message_cache(context(), context()->config()->message_cache_type()));

	// create control socket
	std::string conn_str = "inproc://service_control_" + description();
	zmq_control_socket_m.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_PAIR));

	int timeout = 0;
	zmq_control_socket_m->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
	zmq_control_socket_m->bind(conn_str.c_str());

	// run message dispatch thread
	is_running_m = true;
	thread_m = boost::thread(&handle_t::dispatch_messages, this);

	// connect to hosts 
	lock.unlock();
	connect();
}

handle_t::~handle_t() {
	kill();

	zmq_control_socket_m->close();
	zmq_control_socket_m.reset(NULL);

	thread_m.join();

	log(PLOG_DEBUG, "KILLED HANDLE " + description());
}

void
handle_t::dispatch_messages() {	
	std::string balancer_ident = info_m.as_string() + "." + wuuid_t().generate();
	balancer_t balancer(balancer_ident, endpoints_m, context(), message_cache_m);

	socket_ptr_t control_socket;
	establish_control_conection(control_socket);

	log(PLOG_DEBUG, "started message dispatch for " + description());

	last_response_timer_m.reset();
	deadlined_messages_timer_m.reset();
	control_messages_timer_m.reset();

	// process messages
	while (is_running_m) {
		boost::mutex::scoped_lock lock(mutex_);

		// process incoming control messages every 200 msec
		int control_message = 0;
		              
		if (control_messages_timer_m.elapsed().as_double() > 0.2f) {
		      control_message = receive_control_messages(control_socket, 0);
		      control_messages_timer_m.reset();
		}

		if (control_message == CONTROL_MESSAGE_KILL) {
			// stop message dispatch, finalize everything
			is_running_m = false;
			break;
		}
		else if (control_message > 0) {
			dispatch_control_messages(control_message, balancer);
		}

		// send new message if any
		if (is_running_m && is_connected_m) {
			for (int i = 0; i < 100; ++i) { // batching
				if (message_cache_m->new_messages_count() == 0) {
					break;	
				}

				dispatch_next_available_message(balancer);
			}
		}

		// check for message responces
		bool received_response = false;

		int fast_poll_timeout = 0;		// microsecs
		int long_poll_timeout = 5000;   // microsecs

		int response_poll_timeout = fast_poll_timeout;
		if (last_response_timer_m.elapsed().as_double() > 5.0f) {
			response_poll_timeout = long_poll_timeout;			
		}

		if (is_connected_m && is_running_m) {
			received_response = balancer.check_for_responses(response_poll_timeout);

			// process received responce(s)
			while (received_response) {
				last_response_timer_m.reset();
				response_poll_timeout = fast_poll_timeout;

				lock.unlock();
				dispatch_next_available_response(balancer);
				lock.lock();

				received_response = balancer.check_for_responses(response_poll_timeout);
			}
		}

		if (is_running_m) {
			if (deadlined_messages_timer_m.elapsed().as_double() > 1.0f) {
				lock.unlock();
				process_deadlined_messages();
				lock.lock();
				deadlined_messages_timer_m.reset();
			}
		}
	}

	control_socket.reset();
	log(PLOG_DEBUG, "finished message dispatch for " + description());
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
			message_cache_m->remove_message_from_cache(response->route(), response->uuid());
			enqueue_response(response);
		break;

		case resource_error: {
			if (reshedule_message(response->route(), response->uuid())) {
				std::string message_str = "resheduled msg with uuid: " + response->uuid();
				message_str += " from " + description() + ", type: ERROR";
				message_str += ", error code: ";

				log(PLOG_DEBUG,
					"%s%d, error message: %s",
					message_str.c_str(),
					response->code(),
					response->error_message().c_str());
			}
			else {
				message_cache_m->remove_message_from_cache(response->route(), response->uuid());
				enqueue_response(response);
			}
		}
		break;

		default: {
			message_cache_m->remove_message_from_cache(response->route(), response->uuid());
			enqueue_response(response);

			std::string message_str = "enqued response for msg with uuid: " + response->uuid();
			message_str += " from " + description() + ", type: ERROR";
			message_str += ", error code: ";

			log(PLOG_DEBUG,
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
	if (!is_running_m) {
		return;
	}

	switch (type) {
		case CONTROL_MESSAGE_CONNECT:
			if (!is_connected_m) {
				balancer.connect(endpoints_m);
				is_connected_m = true;
			}
			break;

		case CONTROL_MESSAGE_UPDATE:
			if (is_connected_m) {
				std::vector<cocaine_endpoint> missing_endpoints;
				balancer.update_endpoints(endpoints_m, missing_endpoints);

				for (size_t i = 0; i < missing_endpoints.size(); ++i) {
					message_cache_m->make_all_messages_new_for_route(missing_endpoints.at(i).route_);
				}
			}
			break;

		case CONTROL_MESSAGE_DISCONNECT:
			balancer.disconnect();
			is_connected_m = false;
			break;
	}
}

boost::shared_ptr<message_cache>
handle_t::messages_cache() const {
	return message_cache_m;
}

void
handle_t::process_deadlined_messages() {
	boost::mutex::scoped_lock lock(mutex_);

	assert(message_cache_m);
	message_cache::message_queue_t expired_messages;
	message_cache_m->get_expired_messages(expired_messages);
	
	lock.unlock();

	if (expired_messages.empty()) {
		return;
	}

	for (size_t i = 0; i < expired_messages.size(); ++i) {
		if (!expired_messages.at(i)->ack_received()) {
			if (expired_messages.at(i)->can_retry()) {
				expired_messages.at(i)->increment_retries_count();
				message_cache_m->enqueue_with_priority(expired_messages.at(i));

				log(PLOG_DEBUG, "no ACK, resheduled message " + expired_messages.at(i)->uuid());
			}
			else {
				log(PLOG_DEBUG, "reshedule message policy exceeded, did not receive ACK " + expired_messages.at(i)->uuid());
				cached_response_prt_t new_response;
				new_response.reset(new cached_response_t(expired_messages.at(i)->uuid(),
														 "",
														 expired_messages.at(i)->path(),
														 request_error,
														 "server did not reply with ack in time"));

				enqueue_response(new_response);
			}
		}
		else {
			cached_response_prt_t new_response;
			new_response.reset(new cached_response_t(expired_messages.at(i)->uuid(),
													 "",
													 expired_messages.at(i)->path(),
													 deadline_error,
													 "message expired in service's handle"));
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
		receiving_control_socket_ok_m = true;
	}
}

void
handle_t::enqueue_response(cached_response_prt_t response) {
	if (response_callback_m && is_running_m) {
		response_callback_m(response);
	}
}

int
handle_t::receive_control_messages(socket_ptr_t& control_socket, int poll_timeout) {
	if (!is_running_m) {
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
    	std::string sname = info_m.service_name_;
    	std::string hname = info_m.name_;
    	log("control socket recv failed on " + description());
    }

    return 0;
}

bool
handle_t::dispatch_next_available_message(balancer_t& balancer) {
	// send new message if any
	if (message_cache_m->new_messages_count() == 0) {
		return false;
	}

	boost::shared_ptr<message_iface> new_msg = message_cache_m->get_new_message();
	cocaine_endpoint endpoint;
	if (balancer.send(new_msg, endpoint)) {
		new_msg->mark_as_sent(true);
		message_cache_m->move_new_message_to_sent(endpoint.route_);

		std::string log_msg = "sent msg with uuid: %s to %s";
		log(PLOG_DEBUG, log_msg.c_str(), new_msg->uuid().c_str(), description().c_str());

		return true;
	}
	else {
		log(PLOG_ERROR, "dispatch_next_available_message failed");		
	}

	return false;
}

bool
handle_t::reshedule_message(const std::string& route, const std::string& uuid) {
	boost::shared_ptr<message_iface> msg;
	if (!message_cache_m->get_sent_message(route, uuid, msg)) {
		return false;
	}

	assert(msg);

	if (msg->can_retry()) {
		msg->increment_retries_count();
		
		message_cache_m->move_sent_message_to_new_front(route, uuid);
		return true;
	}

	return false;
}

const handle_info_t&
handle_t::info() const {
	return info_m;
}

void
handle_t::kill() {
	if (!is_running_m) {
		return;
	}

	// kill dispatch thread from the inside
	int control_message = CONTROL_MESSAGE_KILL;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_m->send(message);
}

std::string
handle_t::description() {
	return info_m.as_string();
}

void
handle_t::connect() {
	boost::mutex::scoped_lock lock(mutex_);

	if (!is_running_m || endpoints_m.empty() || is_connected_m) {
		return;
	}

	log(PLOG_DEBUG, "CONNECT HANDLE " + description());

	// connect to hosts
	int control_message = CONTROL_MESSAGE_CONNECT;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_m->send(message);
}

void
handle_t::update_endpoints(const std::vector<cocaine_endpoint>& endpoints) {
	if (!is_running_m || endpoints.empty()) {
		return;
	}

	boost::mutex::scoped_lock lock(mutex_);
	endpoints_m = endpoints;
	lock.unlock();

	log(PLOG_DEBUG, "UPDATE HANDLE " + description());

	// connect to hosts
	int control_message = CONTROL_MESSAGE_UPDATE;
	zmq::message_t message(sizeof(int));
	memcpy((void *)message.data(), &control_message, sizeof(int));
	zmq_control_socket_m->send(message);
}

void
handle_t::disconnect() {
	boost::mutex::scoped_lock lock(mutex_);

	if (!is_running_m) {
		return;
	}

	log(PLOG_DEBUG, "DISCONNECT HANDLE " + description());

	// disconnect from all hosts
	std::string control_message = boost::lexical_cast<std::string>(CONTROL_MESSAGE_DISCONNECT);
	zmq::message_t message(control_message.length());
	memcpy((void *)message.data(), control_message.c_str(), control_message.length());
	zmq_control_socket_m->send(message);
}

void
handle_t::make_all_messages_new() {
	assert (message_cache_m);
	return message_cache_m->make_all_messages_new();
}

void
handle_t::assign_message_queue(const message_cache::message_queue_ptr_t& message_queue) {
	assert (message_cache_m);
	return message_cache_m->append_message_queue(message_queue);
}

void
handle_t::set_responce_callback(responce_callback_t callback) {
	boost::mutex::scoped_lock lock(mutex_);
	response_callback_m = callback;
}

void
handle_t::enqueue_message(const boost::shared_ptr<message_iface>& message) {
	message_cache_m->enqueue(message);
}

} // namespace dealer
} // namespace cocaine
