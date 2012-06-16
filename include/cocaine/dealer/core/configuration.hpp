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

class configuration_t;
	
std::ostream& operator << (std::ostream& out, configuration_t& config);
	
class configuration_t : private boost::noncopyable {
public:
	// map dealer service name to service info
	typedef std::map<std::string, service_info_t> services_list_t;
	static const int current_config_version = 1;

public:
	configuration_t();
	explicit configuration_t(const std::string& path);
	virtual ~configuration_t();
	
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
	boost::uint16_t remote_statistics_port() const;

	const services_list_t& services_list() const;
	bool service_info_by_name(const std::string& name, service_info_t& info) const;
	bool service_info_by_name(const std::string& name) const;
	
	friend std::ostream& operator << (std::ostream& out, configuration_t& config);
	
private:
	void parse_basic_settings(const Json::Value& config_value);
	void parse_logger_settings(const Json::Value& config_value);
	void parse_messages_cache_settings(const Json::Value& config_value);
	void parse_persistant_storage_settings(const Json::Value& config_value);
	void parse_statistics_settings(const Json::Value& config_value);
	void parse_services_settings(const Json::Value& config_value);

private:
	// config
	std::string m_path;
	
	// general
	unsigned long long			m_default_message_deadline;
	enum e_message_cache_type	m_message_cache_type;
	
	// logger
	enum e_logger_type	m_logger_type;
	unsigned int		m_logger_flags;
	std::string			m_logger_file_path;
	std::string			m_logger_syslog_identity;

	// persistent storage
	std::string m_eblob_path;
	uint64_t	m_eblob_blob_size;
	int			m_eblob_sync_interval;
	
	// statistics
	bool		m_statistics_enabled;
	bool		m_remote_statistics_enabled;
	boost::uint16_t	m_remote_statistics_port;

	// services
	services_list_t m_services_list;

	// synchronization
	boost::mutex m_mutex;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CONFIGURATION_HPP_INCLUDED_
