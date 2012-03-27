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

#ifndef _COCAINE_DEALER_CONFIGURATION_HPP_INCLUDED_
#define _COCAINE_DEALER_CONFIGURATION_HPP_INCLUDED_

#include <string>
#include <map>
#include <iostream>

#include <boost/thread/mutex.hpp>
#include <boost/utility.hpp>

#include "json/json.h"

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/core/service_info.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"

namespace cocaine {
namespace dealer {

class configuration;
	
std::ostream& operator << (std::ostream& out, configuration& config);
	
class configuration : private boost::noncopyable {
public:
	// map dealer service name to service info
	typedef std::map<std::string, service_info_t> services_list_t;

public:
	configuration();
	explicit configuration(const std::string& path);
	virtual ~configuration();
	
	void load(const std::string& path);
	
	const std::string& config_path() const;
	unsigned int config_version() const;
	unsigned long long message_timeout() const;
	unsigned long long socket_poll_timeout() const;
	size_t max_message_cache_size() const;
	enum message_cache_type message_cache_type() const;
	
	enum logger_type logger_type() const;
	unsigned int logger_flags() const;
	const std::string& logger_file_path() const;
	const std::string& logger_syslog_name() const;
	
	std::string eblob_path() const;
	int64_t eblob_blob_size() const;
	int eblob_sync_interval() const;
	
	enum autodiscovery_type autodiscovery_type() const;
	std::string multicast_ip() const;
	unsigned short multicast_port() const;
	
	bool is_statistics_enabled() const;
	bool is_remote_statistics_enabled() const;
	DT::port remote_statistics_port() const;

	const services_list_t& services_list() const;
	bool service_info_by_name(const std::string& name, service_info_t& info) const;
	bool service_info_by_name(const std::string& name) const;
	
	friend std::ostream& operator<<(std::ostream& out, configuration& config);
	std::string as_string() const;
	std::string as_json() const;
	
private:
	void parse_basic_settings(const Json::Value& config_value);
	void parse_logger_settings(const Json::Value& config_value);
	void parse_messages_cache_settings(const Json::Value& config_value);
	void parse_persistant_storage_settings(const Json::Value& config_value);
	void parse_autodiscovery_settings(const Json::Value& config_value);
	void parse_statistics_settings(const Json::Value& config_value);
	void parse_services_settings(const Json::Value& config_value);

private:
	// config
	std::string path_;
	unsigned int version_;
	
	// general
	unsigned long long message_timeout_;
	unsigned long long socket_poll_timeout_;
	size_t max_message_cache_size_;
	enum message_cache_type message_cache_type_;
	
	// logger
	enum logger_type logger_type_;
	unsigned int logger_flags_;
	std::string logger_file_path_;
	std::string logger_syslog_name_;

	// persistent storage
	std::string eblob_path_;
	uint64_t eblob_blob_size_;
	int eblob_sync_interval_;
	
	// autodiscovery
	enum autodiscovery_type autodiscovery_type_;
	std::string multicast_ip_;
	unsigned short multicast_port_;
	
	// statistics
	bool is_statistics_enabled_;
	bool is_remote_statistics_enabled_;
	DT::port remote_statistics_port_;

	// services
	services_list_t services_list_;

	// synchronization
	boost::mutex mutex_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CONFIGURATION_HPP_INCLUDED_
