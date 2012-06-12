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

#include <memory>

#include "cocaine/dealer/heartbeats/heartbeats_collector.hpp"
#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"
#include "cocaine/dealer/heartbeats/http_hosts_fetcher.hpp"
#include "cocaine/dealer/cocaine_node_info/cocaine_node_info_parser.hpp"
#include "cocaine/dealer/utils/uuid.hpp"

namespace cocaine {
namespace dealer {

heartbeats_collector::heartbeats_collector(const boost::shared_ptr<context_t>& ctx,
										   bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled) {}

heartbeats_collector::~heartbeats_collector() {
	stop();
}

void
heartbeats_collector::run() {
	log(PLOG_DEBUG, "heartbeats - collector started.");

	// create hosts fetchers
	const std::map<std::string, service_info_t>& services_list = config()->services_list();
	std::map<std::string, service_info_t>::const_iterator it = services_list.begin();
	
	for (; it != services_list.end(); ++it) {
		// create specific host fetcher for service
		hosts_fetcher_ptr fetcher;

		switch (it->second.discovery_type) {
			case AT_FILE:
				fetcher.reset(new file_hosts_fetcher(it->second));
				break;

			case AT_HTTP:
				fetcher.reset(new http_hosts_fetcher(it->second));
				break;

			default: {
				std::string error_msg = "unknown autodiscovery type defined for service ";
				error_msg += "\"" + it->second.name + "\"";
				throw internal_error(error_msg);
			}
		}

		hosts_fetchers_m.push_back(fetcher);
	}

	// ping first time without delay
	ping_services();

	// create hosts pinger
	boost::function<void()> f = boost::bind(&heartbeats_collector::ping_services, this);
	refresher_m.reset(new refresher(f, hosts_retrieval_interval));
}

void
heartbeats_collector::stop() {
	log(PLOG_DEBUG, "heartbeats - collector stopped.");

	// kill hosts pinger
	refresher_m.reset();

	// kill http hosts fetchers
	for (size_t i = 0; i < hosts_fetchers_m.size(); ++i) {
		hosts_fetchers_m[i].reset();
	}
}

void
heartbeats_collector::set_callback(callback_t callback) {
	boost::mutex::scoped_lock lock(mutex_m);
	callback_m = callback;
}

void
heartbeats_collector::ping_services() {
	// for each hosts fetcher
	for (size_t i = 0; i < hosts_fetchers_m.size(); ++i) {
		hosts_fetcher_iface::inetv4_endpoints endpoints;
		service_info_t service_info;

		// get service endpoints list
		try {
			if (hosts_fetchers_m[i]->get_hosts(endpoints, service_info)) {
				for (size_t i = 0; i < endpoints.size(); ++i) {
					if (endpoints[i].port == 0) {
						endpoints[i].port = defaults::control_port;
					}
				}

				if (endpoints.size() == 0) {
					std::string error_msg = "heartbeats - fetcher returned no hosts for service %s";
					log(PLOG_WARNING, error_msg.c_str(), service_info.name.c_str());
				}

				services_endpoints_m[service_info.name] = endpoints;
			}
		}
		catch (const std::exception& ex) {
			std::string error_msg = "heartbeats - failed fo retrieve hosts list, details: %s";
			log(PLOG_ERROR, error_msg.c_str(), ex.what());
		}
		catch (...) {
			std::string error_msg = "heartbeats - failed fo retrieve hosts list, no further details available.";
			log(PLOG_ERROR, error_msg.c_str());
		}
	}

	// collect all endpoints from services
	all_endpoints_m.clear();
	std::map<std::string, inetv4_endpoints>::const_iterator it = services_endpoints_m.begin();

	for (; it != services_endpoints_m.end(); ++it) {
		const hosts_fetcher_iface::inetv4_endpoints& endpoints = it->second;

		for (size_t i = 0; i < endpoints.size(); ++i) {
			all_endpoints_m.insert(endpoints[i]);
		}
	}

	ping_endpoints();
	process_alive_endpoints();
}

void
heartbeats_collector::process_alive_endpoints() {
	const std::map<std::string, service_info_t>& services_list = config()->services_list();
	std::map<std::string, service_info_t>::const_iterator it = services_list.begin();
	
	for (; it != services_list.end(); ++it) {
		// <handle name, endpoints list>
		handles_endpoints_t handles_endpoints;

		std::vector<cocaine_endpoint> endpoints;
		const std::string& service_name = it->first;
		const service_info_t& service_info = it->second;

		// collect alive endpoints for service
		std::map<std::string, inetv4_endpoints>::const_iterator sit;
		sit = services_endpoints_m.find(service_name);

		if (sit == services_endpoints_m.end()) {
			continue;
		}

		const inetv4_endpoints& service_endpoints = sit->second;

		// for each service endpoint obtain metadata if possible
		for (size_t i = 0; i < service_endpoints.size(); ++i) {
			std::map<inetv4_endpoint, cocaine_node_info_t>::const_iterator eit;
			eit = endpoints_metadata_m.find(service_endpoints[i]);

			if (eit == endpoints_metadata_m.end()) {
				continue;
			}

			const cocaine_node_info_t& node_info = eit->second;
			cocaine_node_app_info_t app;
			
			// no such app at endpoint
			if (!node_info.app_by_name(service_info.app, app)) {
				continue;
			}

			// app stoped or no handles at endpoint's app
			if (!app.is_running || app.tasks.size() == 0) {
				continue;
			}

			cocaine_node_app_info_t::application_tasks::const_iterator task_it = app.tasks.begin();
			for (; task_it != app.tasks.end(); ++task_it) {
				cocaine_endpoint ce(task_it->second.endpoint, task_it->second.route);
				
				handles_endpoints_t::iterator hit = handles_endpoints.find(task_it->second.name);
				if (hit != handles_endpoints.end()) {
					hit->second.push_back(ce);
				}
				else {
					std::vector<cocaine_endpoint> endpoints_vec;
					endpoints_vec.push_back(ce);
					handles_endpoints[task_it->second.name] = endpoints_vec;
				}
			}
		}

		log_responded_hosts_handles(service_info, handles_endpoints);

		// pass collected data to callback
		callback_m(service_info, handles_endpoints);
	}
}

void
heartbeats_collector::log_responded_hosts_handles(const service_info_t& service_info,
												  const handles_endpoints_t& handles_endpoints)
{
	handles_endpoints_t::const_iterator it = handles_endpoints.begin();
	for (; it != handles_endpoints.end(); ++it) {
		std::string log_msg = "heartbeats - responded endpoints for handle";
		log(PLOG_DEBUG, log_msg + ": [" + service_info.name + "." + it->first + "]");

		for (size_t i = 0; i < it->second.size(); ++i) {
			log(PLOG_DEBUG, "heartbeats - " + it->second[i].endpoint);
		}
	}
}

void
heartbeats_collector::ping_endpoints() {
	endpoints_metadata_m.clear();

	std::set<inetv4_endpoint>::const_iterator it = all_endpoints_m.begin();

	for (; it != all_endpoints_m.end(); ++it) {
		// request endpoint metadata
		std::string metadata;

		if (!get_metainfo_from_endpoint(*it, metadata)) {
			std::string error_msg = "heartbeats - could not retvieve metainfo from cocaine node: " + it->as_string();
			log(PLOG_WARNING, error_msg);

			continue;
		}

		// parse metadata
		cocaine_node_info_t node_info;
		cocaine_node_info_parser_t parser(context());
		parser.set_host_info(it->host.ip, it->port);

		if (!parser.parse(metadata, node_info)) {
			continue;
		}

		endpoints_metadata_m[*it] = node_info;
	}
}

bool
heartbeats_collector::get_metainfo_from_endpoint(const inetv4_endpoint& endpoint,
												 std::string& response)
{
	// create req socket
	std::auto_ptr<zmq::socket_t> zmq_socket;
	zmq_socket.reset(new zmq::socket_t(*(context()->zmq_context()), ZMQ_REQ));
	std::string ex_err;

	// connect to host
	std::string host_ip_str = nutils::ipv4_to_str(endpoint.host.ip);
	std::string connection_str = "tcp://" + host_ip_str + ":";
	connection_str += boost::lexical_cast<std::string>(endpoint.port);

	int timeout = 0;
	zmq_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));

	wuuid_t uuid;
	uuid.generate();
	zmq_socket->setsockopt(ZMQ_IDENTITY, uuid.str().c_str(), uuid.str().length());
	
	zmq_socket->connect(connection_str.c_str());

	// send request for cocaine metadata
	Json::Value msg(Json::objectValue);
	Json::FastWriter writer;

	msg["version"] = 2;
	msg["action"] = "info";

	std::string info_request = writer.write(msg);
	zmq::message_t message(info_request.length());
	memcpy((void *)message.data(), info_request.c_str(), info_request.length());

	bool sent_request_ok = true;

	try {
		sent_request_ok = zmq_socket->send(message);
	}
	catch (const std::exception& ex) {
		sent_request_ok = false;
		ex_err = ex.what();
	}

	if (!sent_request_ok) {
		// in case of bad send
		std::string error_msg = "heartbeats - could not send metadata request to endpoint: " + endpoint.as_string();
		log(PLOG_ERROR, error_msg + ex_err);

		return false;
	}

	// create polling structure
	zmq_pollitem_t poll_items[1];
	poll_items[0].socket = *zmq_socket;
	poll_items[0].fd = 0;
	poll_items[0].events = ZMQ_POLLIN;
	poll_items[0].revents = 0;

	// poll for responce
	int res = zmq_poll(poll_items, 1, host_socket_ping_timeout);
	if (res <= 0) {
		return false;
	}

	if ((ZMQ_POLLIN & poll_items[0].revents) != ZMQ_POLLIN) {
		return false;
	}

	// receive cocaine control data
	zmq::message_t reply;
	bool received_response_ok = true;

	try {
		received_response_ok = zmq_socket->recv(&reply);
		response = std::string(static_cast<char*>(reply.data()), reply.size());
	}
	catch (const std::exception& ex) {
		received_response_ok = false;
		ex_err = ex.what();
	}

	if (!received_response_ok) {
		std::string error_msg = "heartbeats - could not receive metadata response from endpoint: " + endpoint.as_string();
		log(PLOG_ERROR, error_msg + ex_err);

		return false;
	}

	return true;
}

} // namespace dealer
} // namespace cocaine
