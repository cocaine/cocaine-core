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

#include <msgpack.hpp>

#include <boost/thread.hpp>

#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/core/balancer.hpp"

namespace cocaine {
namespace dealer {

enum e_server_response_code {
	SERVER_RPC_MESSAGE_ACK = 1,
	SERVER_RPC_MESSAGE_CHUNK = 5,
	SERVER_RPC_MESSAGE_ERROR = 6,
	SERVER_RPC_MESSAGE_CHOKE = 7
};

balancer_t::balancer_t(const std::string& identity,
					   const std::vector<cocaine_endpoint>& endpoints,
					   boost::shared_ptr<message_cache> message_cache,
					   const boost::shared_ptr<context_t>& ctx,
					   bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled),
	socket_identity_m(identity),
	endpoints_m(endpoints),
	context_(context),
	message_cache_m(message_cache),
	current_endpoint_index_m(0)
{
	std::sort(endpoints_m.begin(), endpoints_m.end());
	recreate_socket();
}

balancer_t::~balancer_t() {
	disconnect();
}

void
balancer_t::connect(const std::vector<cocaine_endpoint>& endpoints) {
	logger()->log("connect " + socket_identity_m);

	if (endpoints.empty()) {
		return;
	}

	assert(socket_m);

	std::string connection_str;
	try {
		for (size_t i = 0; i < endpoints.size(); ++i) {
			connection_str = endpoints[i].endpoint_;
			socket_m->connect(connection_str.c_str());
		}

		boost::this_thread::sleep(boost::posix_time::milliseconds(100));
	}
	catch (const std::exception& ex) {
		std::string error_msg = "balancer with identity " + socket_identity_m + " could not connect to ";
		error_msg += connection_str + " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}
}

void balancer_t::disconnect() {
	if (!socket_m) {
		return;
	}

	log("disconnect balancer " + socket_identity_m);
	socket_m.reset();
}

void	
balancer_t::update_endpoints(const std::vector<cocaine_endpoint>& endpoints,
							 std::vector<cocaine_endpoint>& missing_endpoints)
{
	log("update_endpoints " + socket_identity_m);

	std::vector<cocaine_endpoint> endpoints_tmp = endpoints;
	std::sort(endpoints_tmp.begin(), endpoints_tmp.end());

	if (std::equal(endpoints_m.begin(), endpoints_m.end(), endpoints_tmp.begin())) {
		log("no changes in endpoints on " + socket_identity_m);
		return;
	}

	std::vector<cocaine_endpoint> new_endpoints;
	get_endpoints_diff(endpoints, new_endpoints, missing_endpoints);

	if (missing_endpoints.empty()) {
		log("new endpoints on " + socket_identity_m);
		connect(new_endpoints);
	}
	else {
		log("missing endpoints on " + socket_identity_m);
		recreate_socket();
		connect(endpoints);
	}

	endpoints_m = endpoints_tmp;
}

void
balancer_t::get_endpoints_diff(const std::vector<cocaine_endpoint>& updated_endpoints,
							   std::vector<cocaine_endpoint>& new_endpoints,
							   std::vector<cocaine_endpoint>& missing_endpoints)
{
	for (size_t i = 0; i < updated_endpoints.size(); ++i) {
		if (false == std::binary_search(endpoints_m.begin(), endpoints_m.end(), updated_endpoints[i])) {
			new_endpoints.push_back(updated_endpoints[i]);
		}
	}

	for (size_t i = 0; i < endpoints_m.size(); ++i) {
		if (false == std::binary_search(updated_endpoints.begin(), updated_endpoints.end(), endpoints_m[i])) {
			missing_endpoints.push_back(endpoints_m[i]);
		}
	}
}

void
balancer_t::recreate_socket() {
	log("recreate_socket " + socket_identity_m);
	int timeout = balancer_t::socket_timeout;
	int64_t hwm = balancer_t::socket_hwm;
	socket_m.reset(new zmq::socket_t(*(context_->zmq_context()), ZMQ_ROUTER));
	socket_m->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
	socket_m->setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
	socket_m->setsockopt(ZMQ_IDENTITY, socket_identity_m.c_str(), socket_identity_m.length());
}

cocaine_endpoint&
balancer_t::get_next_endpoint() {
	if (current_endpoint_index_m < endpoints_m.size() - 1) {
		++current_endpoint_index_m;
	}
	else {
		current_endpoint_index_m = 0;	
	}

	return endpoints_m[current_endpoint_index_m];
}

bool
balancer_t::send(boost::shared_ptr<message_iface>& message, cocaine_endpoint& endpoint) {
	assert(socket_m);

	try {
		// send ident
		endpoint = get_next_endpoint();
		zmq::message_t ident_chunk(endpoint.route_.size());
		memcpy((void *)ident_chunk.data(), endpoint.route_.data(), endpoint.route_.size());

		if (true != socket_m->send(ident_chunk, ZMQ_SNDMORE)) {
			return false;
		}

		// send header
		zmq::message_t empty_message(0);
		if (true != socket_m->send(empty_message, ZMQ_SNDMORE)) {
			return false;
		}

		// send message uuid
		const std::string& uuid = message->uuid();
		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, uuid);
		zmq::message_t uuid_chunk(sbuf.size());
		memcpy((void *)uuid_chunk.data(), sbuf.data(), sbuf.size());

		if (true != socket_m->send(uuid_chunk, ZMQ_SNDMORE)) {
			return false;
		}

		// send message policy
		policy_t server_policy = message->policy().server_policy();

		if (server_policy.deadline > 0.0) {
			// awful semantics! convert deadline [timeout value] to actual [deadline time]
			time_value server_deadline = message->enqued_timestamp();
			server_deadline += server_policy.deadline;
			server_policy.deadline = server_deadline.as_double();
		}

		sbuf.clear();
        msgpack::pack(sbuf, server_policy);
		zmq::message_t policy_chunk(sbuf.size());
		memcpy((void *)policy_chunk.data(), sbuf.data(), sbuf.size());

		if (true != socket_m->send(policy_chunk, ZMQ_SNDMORE)) {
			return false;
		}

		// send data
		size_t data_size = message->size();
		zmq::message_t data_chunk(data_size);

		if (data_size > 0) {
			message->load_data();
			memcpy((void *)data_chunk.data(), message->data(), data_size);
			message->unload_data();
		}

		if (true != socket_m->send(data_chunk)) {
			return false;
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "balancer with identity " + socket_identity_m;
		error_msg += " could not send message, details: ";
		error_msg += ex.what();
		throw internal_error(error_msg);
	}

	return true;
}

bool
balancer_t::check_for_responses(int poll_timeout) const {
	assert(socket_m);

	// poll for responce
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *socket_m;
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

bool
balancer_t::receive_chunk(zmq::message_t& response) {
	try {
		if (socket_m->recv(&response, ZMQ_NOBLOCK) == EAGAIN) {
			return false;
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "balancer with identity " + socket_identity_m;
		error_msg += " — error while receiving response chunk ";
		error_msg += ex.what();
		log(PLOG_DEBUG, error_msg);

		return false;
	}

	return true;
}

bool
balancer_t::receive(boost::shared_ptr<cached_response_t>& response) {
	boost::ptr_vector<zmq::message_t> response_chunks;

	// receive message
	int64_t more = 1;
	size_t more_size = sizeof(more);

	while (more) {
		zmq::message_t* chunk = new zmq::message_t;

		if (!receive_chunk(*chunk)) {
			delete chunk;
			break;
		}
		
		response_chunks.push_back(chunk);

		int __attribute__ ((unused)) rc = zmq_getsockopt(*socket_m, ZMQ_RCVMORE, &more, &more_size);
    	assert (rc == 0);
	}

	if (response_chunks.size() == 0) {
		return false;
	}

	return process_responce(response_chunks, response);
}

bool
balancer_t::process_responce(boost::ptr_vector<zmq::message_t>& chunks,
							 boost::shared_ptr<cached_response_t>& response)
{
	// unpack node identity
	const char* route_chars = reinterpret_cast<const char*>(chunks[0].data());
	std::string route(route_chars, chunks[0].size());

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
	if (!message_cache_m->get_sent_message(route, uuid, sent_msg)) {
		return false;
	}

	std::string message_str = "balancer " + socket_identity_m + " received response for msg with uuid: ";
	message_str += sent_msg->uuid() + ", type: ";

	switch (rpc_code) {
		case SERVER_RPC_MESSAGE_ACK: {
			sent_msg->set_ack_received(true);
			//log(PLOG_DEBUG, message_str + "ACK");
			return false;
		}

		case SERVER_RPC_MESSAGE_CHUNK: {
			//log(PLOG_DEBUG, message_str + "CHUNK");

			response.reset(new cached_response_t(uuid,
												 route,
												 sent_msg->path(),
												 chunks[4].data(),
												 chunks[4].size()));

			response->set_code(response_code::message_chunk);
			return true;
		}
		break;

		case SERVER_RPC_MESSAGE_ERROR: {
			log(PLOG_DEBUG, message_str + "ERROR");

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

			response.reset(new cached_response_t(uuid,
												 route,
												 sent_msg->path(),
												 error_code,
												 error_message));
			return true;
		}
		break;

		case SERVER_RPC_MESSAGE_CHOKE: {
			//log(PLOG_DEBUG, message_str + "CHOKE");

			response.reset(new cached_response_t(uuid,
												 route,
												 sent_msg->path(),
												 NULL,
												 0));

			response->set_code(response_code::message_choke);
			return true;
		}
		break;

		default:
			return false;
	}

	return false;
}

} // namespace dealer
} // namespace cocaine
