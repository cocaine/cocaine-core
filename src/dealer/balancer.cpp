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
					   const std::vector<cocaine_endpoint_t>& endpoints,
					   boost::shared_ptr<message_cache_t> message_cache_t,
					   const boost::shared_ptr<context_t>& ctx,
					   bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled),
	m_endpoints(endpoints),
	m_message_cache(message_cache_t),
	m_current_endpoint_index(0),
	m_socket_identity(identity)
{
	std::sort(m_endpoints.begin(), m_endpoints.end());
	recreate_socket();
}

balancer_t::~balancer_t() {
	disconnect();
}

void
balancer_t::connect(const std::vector<cocaine_endpoint_t>& endpoints) {
	log("connect " + m_socket_identity);

	if (endpoints.empty()) {
		return;
	}

	assert(m_socket);

	std::string connection_str;
	try {
		for (size_t i = 0; i < endpoints.size(); ++i) {
			connection_str = endpoints[i].endpoint;
			m_socket->connect(connection_str.c_str());
		}

		boost::this_thread::sleep(boost::posix_time::milliseconds(100));
	}
	catch (const std::exception& ex) {
		std::string error_msg = "balancer with identity " + m_socket_identity + " could not connect to ";
		error_msg += connection_str + " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}
}

void balancer_t::disconnect() {
	if (!m_socket) {
		return;
	}

	log("disconnect balancer " + m_socket_identity);
	m_socket.reset();
}

void	
balancer_t::update_endpoints(const std::vector<cocaine_endpoint_t>& endpoints,
							 std::vector<cocaine_endpoint_t>& missing_endpoints)
{
	log("update_endpoints " + m_socket_identity);

	std::vector<cocaine_endpoint_t> endpoints_tmp = endpoints;
	std::sort(endpoints_tmp.begin(), endpoints_tmp.end());

	if (m_endpoints.size() == endpoints_tmp.size()) {
		if (std::equal(m_endpoints.begin(), m_endpoints.end(), endpoints_tmp.begin())) {
			log("no changes in endpoints on " + m_socket_identity);
			return;
		}
	}

	std::vector<cocaine_endpoint_t> new_endpoints;
	get_endpoints_diff(endpoints, new_endpoints, missing_endpoints);

	if (missing_endpoints.empty()) {
		log("new endpoints on " + m_socket_identity);
		connect(new_endpoints);
	}
	else {
		log("missing endpoints on " + m_socket_identity);
		recreate_socket();
		connect(endpoints);
	}

	m_endpoints = endpoints_tmp;
}

void
balancer_t::get_endpoints_diff(const std::vector<cocaine_endpoint_t>& updated_endpoints,
							   std::vector<cocaine_endpoint_t>& new_endpoints,
							   std::vector<cocaine_endpoint_t>& missing_endpoints)
{
	for (size_t i = 0; i < updated_endpoints.size(); ++i) {
		if (false == std::binary_search(m_endpoints.begin(), m_endpoints.end(), updated_endpoints[i])) {
			new_endpoints.push_back(updated_endpoints[i]);
		}
	}

	for (size_t i = 0; i < m_endpoints.size(); ++i) {
		if (false == std::binary_search(updated_endpoints.begin(), updated_endpoints.end(), m_endpoints[i])) {
			missing_endpoints.push_back(m_endpoints[i]);
		}
	}
}

void
balancer_t::recreate_socket() {
	log("recreate_socket " + m_socket_identity);
	int timeout = balancer_t::socket_timeout;
	int64_t hwm = balancer_t::socket_hwm;
	m_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_ROUTER));
	m_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
	m_socket->setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
	m_socket->setsockopt(ZMQ_IDENTITY, m_socket_identity.c_str(), m_socket_identity.length());
}

cocaine_endpoint_t&
balancer_t::get_next_endpoint() {
	if (m_current_endpoint_index < m_endpoints.size() - 1) {
		++m_current_endpoint_index;
	}
	else {
		m_current_endpoint_index = 0;	
	}

	return m_endpoints[m_current_endpoint_index];
}

bool
balancer_t::send(boost::shared_ptr<message_iface>& message, cocaine_endpoint_t& endpoint) {
	assert(m_socket);

	try {
		// send ident
		endpoint = get_next_endpoint();
		zmq::message_t ident_chunk(endpoint.route.size());
		memcpy((void *)ident_chunk.data(), endpoint.route.data(), endpoint.route.size());

		if (true != m_socket->send(ident_chunk, ZMQ_SNDMORE)) {
			return false;
		}

		// send header
		zmq::message_t empty_message(0);
		if (true != m_socket->send(empty_message, ZMQ_SNDMORE)) {
			return false;
		}

		// send message uuid
		const std::string& uuid = message->uuid();
		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, uuid);
		zmq::message_t uuid_chunk(sbuf.size());
		memcpy((void *)uuid_chunk.data(), sbuf.data(), sbuf.size());

		if (true != m_socket->send(uuid_chunk, ZMQ_SNDMORE)) {
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

		if (true != m_socket->send(policy_chunk, ZMQ_SNDMORE)) {
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

		if (true != m_socket->send(data_chunk)) {
			return false;
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "balancer with identity " + m_socket_identity;
		error_msg += " could not send message, details: ";
		error_msg += ex.what();
		throw internal_error(error_msg);
	}

	return true;
}

bool
balancer_t::check_for_responses(int poll_timeout) const {
	assert(m_socket);

	// poll for responce
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *m_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	int socket_response = zmq_poll(poll_items, 1, poll_timeout);

	if (socket_response <= 0) {
		return false;
	}

	// in case we received message response_t
	if ((ZMQ_POLLIN & poll_items[0].revents) == ZMQ_POLLIN) {
		return true;
	}

    return false;
}

bool
balancer_t::receive_chunk(zmq::message_t& response_t) {
	try {
		if (m_socket->recv(&response_t, ZMQ_NOBLOCK) == EAGAIN) {
			return false;
		}
	}
	catch (const std::exception& ex) {
		std::string error_msg = "balancer with identity " + m_socket_identity;
		error_msg += " — error while receiving response_t chunk ";
		error_msg += ex.what();
		log(PLOG_DEBUG, error_msg);

		return false;
	}

	return true;
}

bool
balancer_t::receive(boost::shared_ptr<cached_response_t>& response_t) {
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

		int __attribute__ ((unused)) rc = zmq_getsockopt(*m_socket, ZMQ_RCVMORE, &more, &more_size);
    	assert (rc == 0);
	}

	if (response_chunks.size() == 0) {
		return false;
	}

	return process_responce(response_chunks, response_t);
}

bool
balancer_t::process_responce(boost::ptr_vector<zmq::message_t>& chunks,
							 boost::shared_ptr<cached_response_t>& response_t)
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
	if (!m_message_cache->get_sent_message(route, uuid, sent_msg)) {
		return false;
	}

	std::string message_str = "balancer " + m_socket_identity + " received response_t for msg with uuid: ";
	message_str += sent_msg->uuid() + ", type: ";

	switch (rpc_code) {
		case SERVER_RPC_MESSAGE_ACK: {
			sent_msg->set_ack_received(true);
			//log(PLOG_DEBUG, message_str + "ACK");
			return false;
		}

		case SERVER_RPC_MESSAGE_CHUNK: {
			//log(PLOG_DEBUG, message_str + "CHUNK");

			response_t.reset(new cached_response_t(uuid,
												 route,
												 sent_msg->path(),
												 chunks[4].data(),
												 chunks[4].size()));

			response_t->set_code(response_code::message_chunk);
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

			response_t.reset(new cached_response_t(uuid,
												 route,
												 sent_msg->path(),
												 error_code,
												 error_message));
			return true;
		}
		break;

		case SERVER_RPC_MESSAGE_CHOKE: {
			//log(PLOG_DEBUG, message_str + "CHOKE");

			response_t.reset(new cached_response_t(uuid,
												 route,
												 sent_msg->path(),
												 NULL,
												 0));

			response_t->set_code(response_code::message_choke);
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
