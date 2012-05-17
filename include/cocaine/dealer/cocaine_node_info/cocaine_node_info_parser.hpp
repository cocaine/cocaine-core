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

#ifndef _COCAINE_DEALER_COCAINE_NODE_INFO_PARSER_HPP_INCLUDED_
#define _COCAINE_DEALER_COCAINE_NODE_INFO_PARSER_HPP_INCLUDED_

#include <string>

#include "json/json.h"

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_info.hpp"
#include "cocaine/dealer/utils/networking.hpp"

namespace cocaine {
namespace dealer {

class cocaine_node_info_parser_t {
public:
	cocaine_node_info_parser_t() {
		set_host_info(0, 0);
		logger_.reset(new base_logger);
	}

	cocaine_node_info_parser_t(boost::shared_ptr<base_logger> logger) {
		set_host_info(0, 0);
		logger_ = logger;
	}

	~cocaine_node_info_parser_t() {};

	void set_host_info(unsigned int node_ip_address, unsigned short node_port) {
		node_ip_address_ = node_ip_address;
		node_port_ = node_port;

		if (node_ip_address_ == 0 || node_port_ == 0) {
			str_node_adress_ = "[undefined ip:undefined port]";
			return;
		}

		str_node_adress_ = "[" + nutils::ipv4_to_str(node_ip_address);
		str_node_adress_ += ":" + boost::lexical_cast<std::string>(node_port);

		std::string hostname = nutils::hostname_for_ipv4(node_ip_address);
		if (!hostname.empty()) {
			str_node_adress_ += " (" + hostname + ")]";
		}
		else {
			str_node_adress_ += "]";
		}
	}

	void set_host_info(const std::string& node_ip_address, unsigned short node_port) {
		set_host_info(nutils::str_to_ipv4(node_ip_address), node_port);
	}

	bool parse(const std::string& json_string, cocaine_node_info_t& node_info) {
		Json::Value root;
		Json::Reader reader;

		if (!reader.parse(json_string, root)) {
			std::string log_str = "cocaine node %s routing info could not be parsed";
			logger()->log(PLOG_WARNING, log_str.c_str(), str_node_adress_.c_str());
			return false;
		}
	
		// parse apps
		const Json::Value apps = root["apps"];
		if (!apps.isObject() || !apps.size()) {
			std::string log_str = "no apps found in cocaine node %s rounting info";
			logger()->log(PLOG_WARNING, log_str.c_str(), str_node_adress_.c_str());
			return false;
		}

	    Json::Value::Members app_names(apps.getMemberNames());
	    for (Json::Value::Members::iterator it = app_names.begin(); it != app_names.end(); ++it) {
	    	std::string parsed_app_name(*it);
	    	Json::Value json_app_data(apps[parsed_app_name]);

	    	cocaine_node_app_info_t app_info(parsed_app_name);
	    	if (!parse_app_info(json_app_data, app_info)) {
	    		continue;
	    	}
	    	else {
	    		node_info.apps[parsed_app_name] = app_info;
	    	}
	    }

	    // parse remaining properties
	    const Json::Value jobs_props = root["jobs"];
	    if (!jobs_props.isObject()) {
	    	std::string log_str = "no jobs object found in cocaine node %s rounting info";
			logger()->log(PLOG_WARNING, log_str.c_str(), str_node_adress_.c_str());
	    }
	    else {
	    	node_info.pending_jobs = jobs_props.get("pending", 0).asInt();
	    	node_info.processed_jobs = jobs_props.get("processed", 0).asInt();
	    }

	    node_info.route = root.get("route", "").asString();
		node_info.uptime = root.get("uptime", 0.0f).asDouble();
		node_info.ip_address = node_ip_address_;
		node_info.port = node_port_;

		return true;
	}

private:
	bool parse_app_info(const Json::Value& json_app_data, cocaine_node_app_info_t& app_info) {
		// parse tasks
		Json::Value tasks(json_app_data["tasks"]);
    	if (!tasks.isObject() || !tasks.size()) {
        	std::string log_str = "no tasks info for app [" + app_info.name;
	    	log_str += "] found in cocaine node %s rounting info";
			logger()->log(PLOG_WARNING, log_str.c_str(), str_node_adress_.c_str());

			return false;
		}

		Json::Value::Members tasks_names(tasks.getMemberNames());
    	for (Json::Value::Members::iterator it = tasks_names.begin(); it != tasks_names.end(); ++it) {
    		std::string task_name(*it);
    		Json::Value task(tasks[task_name]);

    		if (!task.isObject() || !task.size()) {
    			std::string log_str = "no task info for app [" + app_info.name;
	    		log_str += "], task [" + task_name + "] found in cocaine node %s rounting info";
				logger()->log(PLOG_WARNING, log_str.c_str(), str_node_adress_.c_str());
				continue;
			}

			cocaine_node_task_info_t task_info(task_name);
			if (!parse_task_info(task, task_info)) {
				continue;
			}
			else {
				app_info.tasks[task_name] = task_info;
			}
		}

		// parse remaining properties
		app_info.queue_depth = json_app_data.get("queue-depth", 0).asInt();
		app_info.is_running = json_app_data.get("running", false).asBool();

		const Json::Value slaves_props = json_app_data["slaves"];
	    if (!slaves_props.isObject()) {
	    	std::string log_str = "no slaves info for app [" + app_info.name;
	    	log_str += "] found in cocaine node %s rounting info";
			logger()->log(PLOG_WARNING, log_str.c_str(), str_node_adress_.c_str());
	    }
	    else {
	    	app_info.slaves_busy = slaves_props.get("busy", 0).asInt();
	    	app_info.slaves_total = slaves_props.get("total", 0).asInt();
	    }

		return true;
	}

	bool parse_task_info(const Json::Value& json_app_data, cocaine_node_task_info_t& task_info) {
		std::string task_type = json_app_data.get("type", "").asString();
    	if (task_type != "native-server") {
    		return false;
    	}

    	task_info.backlog = json_app_data.get("backlog", 0).asInt();
	    task_info.endpoint = json_app_data.get("endpoint", "").asString();
	    task_info.route = json_app_data.get("route", "").asString();


		const Json::Value stats_props = json_app_data["stats"];
	    if (stats_props.isObject()) {
	    	task_info.median_processing_time = stats_props.get("median-processing-time", 0).asDouble();
	    	task_info.median_wait_time = stats_props.get("median-wait-time", 0).asDouble();
	    	task_info.time_spent_in_queues = stats_props.get("time-spent-in-queues", 0).asDouble();
	    	task_info.time_spent_on_slaves = stats_props.get("time-spent-on-slaves", 0).asDouble();
	    }

		return true;
	}

	boost::shared_ptr<base_logger> logger() {
		return logger_;
	}

	unsigned int node_ip_address_;
	unsigned short node_port_;
	std::string str_node_adress_;
	boost::shared_ptr<base_logger> logger_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_NODE_INFO_PARSER_HPP_INCLUDED_
