//
// Copyright (C) 2011 Rim Zaidullin <creator@bash.org.ru>
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

#include <cstring>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <zmq.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/details/error.hpp"
#include "cocaine/dealer/details/host_info.hpp"
#include "cocaine/dealer/details/smart_logger.hpp"
#include "cocaine/dealer/details/statistics_collector.hpp"

namespace cocaine {
namespace dealer {

statistics_collector::statistics_collector(boost::shared_ptr<configuration> config,
										   boost::shared_ptr<zmq::context_t> zmq_context) :
	is_enabled_(false),
	config_(config),
	zmq_context_(zmq_context),
	is_running_(false)
{
	if (!config_) {
		std::string error_str = "configuration object is empty";
		error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_str);
	}

	if (!zmq_context_) {
		std::string error_str = "zmq context object is empty";
		error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_str);
	}

	logger_.reset(new smart_logger<empty_logger>);

	init();
}

statistics_collector::statistics_collector(boost::shared_ptr<configuration> config,
										   boost::shared_ptr<zmq::context_t> zmq_context,
										   boost::shared_ptr<base_logger> logger) :
	is_enabled_(false),
	config_(config),
	zmq_context_(zmq_context),
	is_running_(false)
{
	if (!config_) {
		std::string error_str = "configuration object is empty";
		error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_str);
	}

	if (!zmq_context_) {
		std::string error_str = "zmq context object is empty";
		error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_str);
	}

	set_logger(logger);
	init();
}

statistics_collector::~statistics_collector() {
	is_running_ = false;
	thread_.join();
}

void
statistics_collector::init() {
	is_enabled_ = config_->is_statistics_enabled();

	used_cache_size_ = 0;

	if (config_->is_remote_statistics_enabled()) {
		// run main thread
		is_running_ = true;
		thread_ = boost::thread(&statistics_collector::process_remote_connection, this);
	}
}

void
statistics_collector::set_logger(boost::shared_ptr<base_logger> logger) {
	if (logger) {
		logger_ = logger;
	}
	else {
		logger_.reset(new smart_logger<empty_logger>);
	}
}

boost::shared_ptr<base_logger>
statistics_collector::logger() {
	boost::mutex::scoped_lock lock(mutex_);
	return logger_;
}

boost::shared_ptr<configuration>
statistics_collector::config() const {
	return config_;
}

void
statistics_collector::process_remote_connection() {
	// create zmq resp socket
	zmq::socket_t socket(*zmq_context_, ZMQ_REP);
	DT::port port = config()->remote_statistics_port();
	std::string port_str = boost::lexical_cast<std::string>(port);
	socket.bind(("tcp://*:" + port_str).c_str());

	while (is_running_) {
		// poll for request
		zmq_pollitem_t poll_items[1];
		poll_items[0].socket = socket;
		poll_items[0].fd = 0;
		poll_items[0].events = ZMQ_POLLIN;
		poll_items[0].revents = 0;

		int socket_response = zmq_poll(poll_items, 1, 0);
		if (socket_response <= 0) {
			continue;
		}

		// in case we received control message
	    if ((ZMQ_POLLIN & poll_items[0].revents) != ZMQ_POLLIN) {
	    	continue;
	    }

    	zmq::message_t request;

    	// see if we're still running
    	if (!is_running_) {
    		continue;
    	}

    	std::string response_json;

    	try {
    		if(!socket.recv(&request)) {
    			logger()->log("recv failed at " + std::string(BOOST_CURRENT_FUNCTION));
    			continue;
    		}
    		else {
				std::string request_str((char*)request.data(), request.size());
				response_json = process_request_json(request_str);
    		}
    	}
    	catch (const std::exception& ex) {
			std::string error_msg = "some very ugly shit happend while recv on socket at ";
			error_msg += std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " details: " + std::string(ex.what());
			throw error(error_msg);
    	}

    	// see if we're still running
    	if (!is_running_ || response_json.empty()) {
    		continue;
    	}

    	// send response
		try {
			// send response
			size_t data_len = response_json.length();

			zmq::message_t reply(data_len);
			memcpy((void*)reply.data(), response_json.c_str(), data_len);

			if(!socket.send(reply)) {
				logger()->log("sending failed at " + std::string(BOOST_CURRENT_FUNCTION));
			}
		}
		catch (const std::exception& ex) {
			std::string error_msg = "some very ugly shit happend while send on socket at ";
			error_msg += std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " details: " + std::string(ex.what());
			throw error(error_msg);
		}
	}
}

std::string
statistics_collector::get_error_json(enum statictics_req_error err) const {
	Json::Value error_json(Json::objectValue);
	Json::FastWriter writer;

	switch (err) {
		case SRE_BAD_JSON_ERROR:
			error_json["error"] = (int)SRE_BAD_JSON_ERROR;
			error_json["message"] = "could not parse statistics request json";
			break;

		case SRE_NO_VERSION_ERROR:
			error_json["error"] = (int)SRE_NO_VERSION_ERROR;
			error_json["message"] = "could find version field in statistics request json";
			break;

		case SRE_UNSUPPORTED_VERSION_ERROR:
			error_json["error"] = (int)SRE_UNSUPPORTED_VERSION_ERROR;
			error_json["message"] = "unsupported protocol version in statistics request json";
			break;

		case SRE_NO_ACTION_ERROR:
			error_json["error"] = (int)SRE_NO_ACTION_ERROR;
			error_json["message"] = "could find action field in statistics request json";
			break;

		case SRE_UNSUPPORTED_ACTION_ERROR:
			error_json["error"] = (int)SRE_UNSUPPORTED_ACTION_ERROR;
			error_json["message"] = "unsupported action in statistics request json";
			break;
	}

	return writer.write(error_json);
}

std::string
statistics_collector::cache_stats_json() const {
	Json::FastWriter writer;
	Json::Value root;
	root["1 - max cache size"] = (unsigned int)config()->max_message_cache_size();
	root["2 - used bytes"] = (unsigned int)used_cache_size_;
	root["3 - free bytes"] = (unsigned int)(config()->max_message_cache_size() - used_cache_size_);

	return writer.write(root);
}

std::string
statistics_collector::all_services_json() {
	boost::mutex::scoped_lock lock(mutex_);

	Json::FastWriter writer;
	Json::Value root;

	// cache info
	Json::Value cache_info;
	cache_info["1 - max cache size"] = (unsigned int)config()->max_message_cache_size();
	cache_info["2 - used bytes"] = (unsigned int)used_cache_size_;
	cache_info["3 - free bytes"] = (unsigned int)(config()->max_message_cache_size() - used_cache_size_);
	root["1 - cache info"] = cache_info;

	// queues totals info
	size_t total_queued_messages = 0;
	size_t total_unhandled_messages = 0;

	Json::Value messages_statistics;
	typedef configuration::services_list_t slist_t;
	const slist_t& services = config()->services_list();

	// get unhandled messages total
	slist_t::const_iterator services_it = services.begin();
	for (; services_it != services.end(); ++services_it) {

		// get service stats
		services_stats_t::iterator service_stat_it = services_stats_.find(services_it->second.name_);
		if (service_stat_it != services_stats_.end()) {
			std::map<std::string, size_t>& umsgs = service_stat_it->second.unhandled_messages;

			// unhandled messages
			std::map<std::string, size_t>::iterator uit = umsgs.begin();
			for (; uit != umsgs.end(); ++uit) {
				total_unhandled_messages += uit->second;
			}
		}
	}
	messages_statistics["1 - queued"] = (unsigned int)total_queued_messages;
	messages_statistics["2 - unhandled"] = (unsigned int)total_unhandled_messages;
	root["2 - messages statistics"] = messages_statistics;

	// services
	services_it = services.begin();
	for (; services_it != services.end(); ++services_it) {
		Json::Value service_info;
		service_info["1 - cocaine app"] = services_it->second.app_name_;
		service_info["2 - instance"] = services_it->second.instance_;
		service_info["3 - description"] = services_it->second.description_;
		service_info["4 - control port"] = services_it->second.hosts_url_;
		service_info["5 - hosts url"] = services_it->second.hosts_url_;

		// get service stats
		services_stats_t::iterator service_stat_it = services_stats_.find(services_it->second.name_);
		if (service_stat_it != services_stats_.end()) {
			// get references
			std::map<DT::ip_addr, std::string>& hosts = service_stat_it->second.hosts;
			std::map<std::string, size_t>& umsgs = service_stat_it->second.unhandled_messages;
			std::vector<std::string>& handles = service_stat_it->second.handles;

			// unhandled messages
			size_t unhandled_count = 0;
			std::map<std::string, size_t>::iterator uit = umsgs.begin();
			for (; uit != umsgs.end(); ++uit) {
				unhandled_count += uit->second;
			}
			service_info["6 - unhandled messages count"] = (unsigned int)unhandled_count;

			// service handles
			if (handles.empty()) {
				service_info["7 - handles"] = "no handles";
			}
			else {
				Json::Value service_handles;

				for (size_t i = 0; i < handles.size(); ++i) {

					// get handle statistics
					std::pair<std::string, std::string> stat_key(services_it->second.name_, handles[i]);
					handle_stats_t::iterator handle_it = handles_stats_.find(stat_key);

					if (handle_it != handles_stats_.end()) {
						Json::Value handle_info;

						handle_info["01 - queue pending"] = (unsigned int)handle_it->second.queue_status.pending;
						handle_info["02 - queue sent"] = (unsigned int)handle_it->second.queue_status.sent;
						handle_info["03 - overall sent"] = (unsigned int)handle_it->second.sent_messages;
						handle_info["04 - resent"] = (unsigned int)handle_it->second.resent_messages;
						handle_info["05 - bad sent"] = (unsigned int)handle_it->second.bad_sent_messages;
						handle_info["06 - timedout"] = (unsigned int)handle_it->second.timedout_responces;
						handle_info["07 - all responces"] = (unsigned int)handle_it->second.all_responces;
						handle_info["08 - good responces"] = (unsigned int)handle_it->second.normal_responces;
						handle_info["09 - err responces"] = (unsigned int)handle_it->second.err_responces;
						handle_info["10 - expired"] = (unsigned int)handle_it->second.expired_responses;

						service_handles[handles[i]] = handle_info;
					}
					else {
						service_handles[handles[i]] = "no handle statistics";
					}
				}

				service_info["7 - handles"] = service_handles;
			}

			// active hosts list
			if (hosts.empty()){
				service_info["8 - active hosts"] = "no hosts";
			}
			else {
				Json::Value service_hosts;
				std::map<DT::ip_addr, std::string>::iterator hosts_it = hosts.begin();
				size_t counter = 1;
				for (; hosts_it != hosts.end(); ++hosts_it) {
					std::string key = "host " + boost::lexical_cast<std::string>(counter);
					std::string value = host_info<DT>::string_from_ip(hosts_it->first);
					value += "(" + hosts_it->second + ")";
					service_hosts[key] = value;
					++counter;
				}

				service_info["8 - active hosts"] = service_hosts;
			}
		}

		Json::Value service;
		service[services_it->first] = service_info;
		root["3 - services"] = service;
	}

	return writer.write(root);
}

std::string
statistics_collector::process_request_json(const std::string& request_json) {
	// parse request json
	Json::Value root;
	Json::Reader reader;
	bool parsing_successful = reader.parse(request_json, root);

	if (!parsing_successful) {
		return get_error_json(SRE_BAD_JSON_ERROR);
	}

	// check protocol version
	int version = root.get("version", 1239846245).asUInt();

	if (version != STATISTICS_PROTOCOL_VERSION) {
		if (version == 1239846245) {
			return get_error_json(SRE_NO_VERSION_ERROR);
		}
		else {
			return get_error_json(SRE_UNSUPPORTED_VERSION_ERROR);
		}
	}

	// check action
	std::string action = root.get("action", "1239846245").asString();
	if (action == "1239846245") {
		return get_error_json(SRE_NO_ACTION_ERROR);
	}

	if (action != "cache_stats" &&
		action != "config" &&
		action != "all_services" &&
		action != "service") {
		return get_error_json(SRE_UNSUPPORTED_ACTION_ERROR);
	}

	// get cache stats
	if (action == "cache_stats") {
		return cache_stats_json();
	}

	// get all config data
	if (action == "config") {
		return config()->as_json();
	}

	// get all services data
	if (action == "all_services") {
		return all_services_json();
	}

	return "";
}

std::string
statistics_collector::as_json() const {
	Json::FastWriter writer;
	Json::Value root;

	typedef configuration::services_list_t slist_t;
	const slist_t& services = config()->services_list();

	slist_t::const_iterator it = services.begin();
	for (; it != services.end(); ++it) {
		Json::Value service_info;
		service_info["cocaine app"] = it->second.app_name_;
		service_info["description"] = it->second.description_;
		service_info["instance"] = it->second.instance_;
		service_info["hosts url"] = it->second.hosts_url_;
		service_info["control port"] = it->second.hosts_url_;

		Json::Value service;
		service[it->first] = service_info;
		root["services"] = service;
	}

	return writer.write(root);
}

void
statistics_collector::enable(bool value) {
	is_enabled_ = value;
}

void
statistics_collector::update_used_cache_size(size_t used_cache_size) {
	if (!is_enabled_) {
		return;
	}

	boost::mutex::scoped_lock lock(mutex_);
	used_cache_size_ = used_cache_size;
}

void
statistics_collector::update_service_stats(const std::string& service_name, const service_stats& stats) {
	if (!is_enabled_) {
		return;
	}

	boost::mutex::scoped_lock lock(mutex_);
	services_stats_[service_name] = stats;
}

void
statistics_collector::update_handle_stats(const std::string& service,
										  const std::string& handle,
										  const handle_stats& stats)
{
	if (!is_enabled_) {
		return;
	}

	boost::mutex::scoped_lock lock(mutex_);
	handles_stats_[std::make_pair(service, handle)] = stats;
}

bool
statistics_collector::get_handle_stats(const std::string& service,
						  			   const std::string& handle,
						  			   handle_stats& stats)
{
	if (!is_enabled_) {
		return false;
	}

	boost::mutex::scoped_lock lock(mutex_);
	handle_stats_t::iterator it = handles_stats_.find(std::make_pair(service, handle));

	if (it == handles_stats_.end()) {
		return false;	
	}

	stats = it->second;
	return true;
}

} // namespace dealer
} // namespace cocaine
