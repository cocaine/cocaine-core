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

#include <memory>

#include "cocaine/dealer/heartbeats/heartbeats_collector.hpp"
#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"
#include "cocaine/dealer/heartbeats/http_hosts_fetcher.hpp"
#include "cocaine/dealer/cocaine_node_info/cocaine_node_info_parser.hpp"

namespace cocaine {
namespace dealer {

heartbeats_collector::heartbeats_collector(boost::shared_ptr<configuration> config,
										   boost::shared_ptr<zmq::context_t> zmq_context) :
	config_(config),
	zmq_context_(zmq_context)
{
	logger_.reset(new base_logger);
}

heartbeats_collector::~heartbeats_collector() {
	stop();
}

void
heartbeats_collector::run() {
	logger_->log(PLOG_DEBUG, "heartbeats - collector started.");

	// create hosts fetchers
	const std::map<std::string, service_info_t>& services_list = config_->services_list();
	std::map<std::string, service_info_t>::const_iterator it = services_list.begin();
	
	for (; it != services_list.end(); ++it) {
		// create specific host fetcher for service
		hosts_fetcher_ptr fetcher;

		switch (it->second.discovery_type_) {
			case AT_FILE:
				fetcher.reset(new file_hosts_fetcher(it->second));
				break;

			case AT_HTTP:
				fetcher.reset(new http_hosts_fetcher(it->second));
				break;

			default: {
				std::string error_msg = "unknown autodiscovery type defined for service ";
				error_msg += "\"" + it->second.name_ + "\"";
				throw internal_error(error_msg);
			}
		}

		hosts_fetchers_.push_back(fetcher);
	}

	// ping first time without delay
	ping_services();

	// create hosts pinger
	boost::function<void()> f = boost::bind(&heartbeats_collector::ping_services, this);
	refresher_.reset(new refresher(f, hosts_ping_timeout));
}

void
heartbeats_collector::stop() {
	logger_->log(PLOG_DEBUG, "heartbeats - collector stopped.");

	// kill hosts pinger
	refresher_.reset();

	// kill http hosts fetchers
	for (size_t i = 0; i < hosts_fetchers_.size(); ++i) {
		hosts_fetchers_[i].reset();
	}
}

void
heartbeats_collector::set_callback(callback_t callback) {
	boost::mutex::scoped_lock lock(mutex_);
	callback_ = callback;
}

void
heartbeats_collector::ping_services() {
	// for each hosts fetcher
	for (size_t i = 0; i < hosts_fetchers_.size(); ++i) {
		hosts_fetcher_iface::inetv4_endpoints endpoints;
		service_info_t service_info;

		// get service endpoints list
		try {
			if (hosts_fetchers_[i]->get_hosts(endpoints, service_info)) {
				for (size_t i = 0; i < endpoints.size(); ++i) {
					if (endpoints[i].port_ == 0) {
						endpoints[i].port_ = defaults::control_port;
					}
				}

				if (endpoints.size() == 0) {
					std::string error_msg = "heartbeats - fetcher returned no hosts for service %s";
					logger_->log(PLOG_WARNING, error_msg.c_str(), service_info.name_.c_str());
				}

				services_endpoints_[service_info.name_] = endpoints;
			}
		}
		catch (const internal_error& err) {
			std::string error_msg = "heartbeats - failed fo retrieve hosts list at: %s, details: %s";
			logger_->log(PLOG_ERROR, error_msg.c_str(),
						 std::string(BOOST_CURRENT_FUNCTION).c_str(),
						 err.what());
		}
		catch (const std::exception& ex) {
			std::string error_msg = "heartbeats - failed fo retrieve hosts list at: %s, details: %s";
			logger_->log(PLOG_ERROR, error_msg.c_str(),
						 std::string(BOOST_CURRENT_FUNCTION).c_str(),
						 ex.what());
		}
		catch (...) {
			std::string error_msg = "heartbeats - failed fo retrieve hosts list at: %s, no further details available.";
			logger_->log(PLOG_ERROR, error_msg.c_str(), std::string(BOOST_CURRENT_FUNCTION).c_str());
		}
	}

	// collect all endpoints from services
	all_endpoints_.clear();
	std::map<std::string, inetv4_endpoints>::const_iterator it = services_endpoints_.begin();

	for (; it != services_endpoints_.end(); ++it) {
		const hosts_fetcher_iface::inetv4_endpoints& endpoints = it->second;

		for (size_t i = 0; i < endpoints.size(); ++i) {
			all_endpoints_.insert(endpoints[i]);
		}
	}

	ping_endpoints();
	process_alive_endpoints();
}

void
heartbeats_collector::process_alive_endpoints() {
	const std::map<std::string, service_info_t>& services_list = config_->services_list();
	std::map<std::string, service_info_t>::const_iterator it = services_list.begin();
	
	for (; it != services_list.end(); ++it) {
		const std::string& service_name = it->first;
		const service_info_t& service_info = it->second;

		// <handle, endpoint> - endpoints for each handle of the service
		std::multimap<std::string, cocaine_endpoint> handles_endpoints;

		// collect alive endpoints for service
		std::map<std::string, inetv4_endpoints>::const_iterator sit;
		sit = services_endpoints_.find(service_name);

		if (sit == services_endpoints_.end()) {
			continue;
		}

		const inetv4_endpoints& service_endpoints = sit->second;

		// for each service endpoint obtain metadata if possible
		for (size_t i = 0; i < service_endpoints.size(); ++i) {
			std::map<inetv4_endpoint, cocaine_node_info>::const_iterator eit;
			eit = endpoints_metadata_.find(service_endpoints[i]);

			if (eit == endpoints_metadata_.end()) {
				continue;
			}

			const cocaine_node_info& node_info = eit->second;
			cocaine_node_app_info app;
			
			// no such app at endpoint
			if (!node_info.app_by_name(service_info.app_, app)) {
				continue;
			}

			// app stoped or no handles at endpoint's app
			if (!app.is_running || app.tasks.size() == 0) {
				continue;
			}

			cocaine_node_app_info::application_tasks::const_iterator task_it = app.tasks.begin();
			for (; task_it != app.tasks.end(); ++task_it) {
				cocaine_endpoint cp;
				cp.endpoint_ = task_it->second.endpoint;
				cp.route_ = task_it->second.route;
				handles_endpoints.insert(std::make_pair(task_it->second.name, cp));
			}
		}

		log_responded_hosts_handles(service_info, handles_endpoints);

		// pass collected data to callback
		boost::mutex::scoped_lock lock(mutex_);
		callback_(service_info, handles_endpoints);
	}
}

void
heartbeats_collector::log_responded_hosts_handles(const service_info_t& s_info,
												  const std::multimap<std::string, cocaine_endpoint>& handles_endpoints)
{
	std::string log_msg = "heartbeats - responded endpoints for handle";
	std::set<std::string> handles;
	
	std::multimap<std::string, cocaine_endpoint>::const_iterator it = handles_endpoints.begin();
	for (; it != handles_endpoints.end(); ++it) {
		handles.insert(it->first);
	}

	for (std::set<std::string>::const_iterator it = handles.begin(); it != handles.end(); ++it) {
		logger_->log(PLOG_DEBUG, log_msg + ": [" + s_info.name_ + "." + *it + "]");

		std::multimap<std::string, cocaine_endpoint>::const_iterator eit, it_begin, it_end;
		boost::tie(it_begin, it_end) = handles_endpoints.equal_range(*it);

		for (eit = it_begin; eit != it_end; ++eit) {
			logger_->log(PLOG_DEBUG, "heartbeats - " + eit->second.endpoint_);
		}
	}
}

void
heartbeats_collector::ping_endpoints() {
	endpoints_metadata_.clear();

	std::set<inetv4_endpoint>::const_iterator it = all_endpoints_.begin();

	for (; it != all_endpoints_.end(); ++it) {
		// request endpoint metadata
		std::string metadata;

		if (!get_metainfo_from_endpoint(*it, metadata)) {
			std::string error_msg = "heartbeats - could not retvieve metainfo from cocaine node: " + it->as_string();
			logger_->log(PLOG_WARNING, error_msg);

			continue;
		}

		// parse metadata
		cocaine_node_info node_info;
		cocaine_node_info_parser parser(logger_);
		parser.set_host_info(it->host_.ip_, it->port_);

		if (!parser.parse(metadata, node_info)) {
			continue;
		}

		endpoints_metadata_[*it] = node_info;
	}
}

bool
heartbeats_collector::get_metainfo_from_endpoint(const inetv4_endpoint& endpoint,
												 std::string& response)
{
	// create req socket
	std::auto_ptr<zmq::socket_t> zmq_socket;
	zmq_socket.reset(new zmq::socket_t(*(zmq_context_), ZMQ_REQ));
	std::string ex_err;

	// connect to host
	std::string host_ip_str = nutils::ipv4_to_str(endpoint.host_.ip_);
	std::string connection_str = "tcp://" + host_ip_str + ":";
	connection_str += boost::lexical_cast<std::string>(endpoint.port_);

	int timeout = 0;
	zmq_socket->setsockopt(ZMQ_LINGER, &timeout, sizeof(timeout));
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
		logger_->log(PLOG_ERROR, error_msg + ex_err);

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
		logger_->log(PLOG_ERROR, error_msg + ex_err);

		return false;
	}

	return true;
}

void
heartbeats_collector::set_logger(boost::shared_ptr<base_logger> logger) {
	logger_ = logger;
}

} // namespace dealer
} // namespace cocaine
