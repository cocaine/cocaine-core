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

#ifndef _COCAINE_DEALER_CONFIGURATION_HPP_INCLUDED_
#define _COCAINE_DEALER_CONFIGURATION_HPP_INCLUDED_

#include <string>
#include <map>
#include <iostream>

#include <boost/thread/mutex.hpp>
#include <boost/utility.hpp>

#include "json/json.h"

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/defaults.hpp"
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
	static const int current_config_version = 1;

public:
	configuration();
	explicit configuration(const std::string& path);
	virtual ~configuration();
	
	void load(const std::string& path);
	
	const std::string& config_path() const;
	unsigned int config_version() const;
	unsigned long long default_message_deadline() const;
	unsigned long long socket_poll_timeout() const;
	enum e_message_cache_type message_cache_type() const;
	
	enum e_logger_type logger_type() const;
	unsigned int logger_flags() const;
	const std::string& logger_file_path() const;
	const std::string& logger_syslog_identity() const;
	
	std::string eblob_path() const;
	int64_t eblob_blob_size() const;
	int eblob_sync_interval() const;
	
	bool is_statistics_enabled() const;
	bool is_remote_statistics_enabled() const;
	DT::port remote_statistics_port() const;

	const services_list_t& services_list() const;
	bool service_info_by_name(const std::string& name, service_info_t& info) const;
	bool service_info_by_name(const std::string& name) const;
	
	friend std::ostream& operator << (std::ostream& out, configuration& config);
	
private:
	void parse_basic_settings(const Json::Value& config_value);
	void parse_logger_settings(const Json::Value& config_value);
	void parse_messages_cache_settings(const Json::Value& config_value);
	void parse_persistant_storage_settings(const Json::Value& config_value);
	void parse_statistics_settings(const Json::Value& config_value);
	void parse_services_settings(const Json::Value& config_value);

private:
	// config
	std::string path_m;
	
	// general
	unsigned long long			default_message_deadline_m;
	enum e_message_cache_type	message_cache_type_m;
	
	// logger
	enum e_logger_type	logger_type_m;
	unsigned int		logger_flags_m;
	std::string			logger_file_path_m;
	std::string			logger_syslog_identity_m;

	// persistent storage
	std::string eblob_path_m;
	uint64_t	eblob_blob_size_m;
	int			eblob_sync_interval_;
	
	// statistics
	bool		is_statistics_enabled_m;
	bool		is_remote_statistics_enabled_m;
	DT::port	remote_statistics_port_m;

	// services
	services_list_t services_list_m;

	// synchronization
	boost::mutex mutex_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CONFIGURATION_HPP_INCLUDED_
