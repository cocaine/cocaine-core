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

#include <fstream>
#include <stdexcept>
#include <sstream>

#include <boost/current_function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/core/configuration.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"

namespace cocaine {
namespace dealer {

configuration_t::configuration_t() :
	m_default_message_deadline(defaults_t::default_message_deadline),
	m_message_cache_type(defaults_t::message_cache_type),
	m_logger_type(defaults_t::logger_type),
	m_logger_flags(defaults_t::logger_flags),
	m_eblob_path(defaults_t::eblob_path),
	m_eblob_blob_size(defaults_t::eblob_blob_size),
	m_eblob_sync_interval(defaults_t::eblob_sync_interval),
	m_statistics_enabled(false),
	m_remote_statistics_enabled(false),
	m_remote_statistics_port(defaults_t::statistics_port)
{
	
}

configuration_t::configuration_t(const std::string& path) :
	m_path(path),
	m_default_message_deadline(defaults_t::default_message_deadline),
	m_message_cache_type(defaults_t::message_cache_type),
	m_logger_type(defaults_t::logger_type),
	m_logger_flags(defaults_t::logger_flags),
	m_eblob_path(defaults_t::eblob_path),
	m_eblob_blob_size(defaults_t::eblob_blob_size),
	m_eblob_sync_interval(defaults_t::eblob_sync_interval),
	m_statistics_enabled(false),
	m_remote_statistics_enabled(false),
	m_remote_statistics_port(defaults_t::statistics_port)
{
	load(path);
}

configuration_t::~configuration_t() {
	
}

void
configuration_t::parse_logger_settings(const Json::Value& config_value) {
	const Json::Value logger_value = config_value["logger"];
	std::string log_type = logger_value.get("type", "STDOUT_LOGGER").asString();

	if (log_type == "STDOUT_LOGGER") {
		m_logger_type = STDOUT_LOGGER;
	}
	else if (log_type == "FILE_LOGGER") {
		m_logger_type = FILE_LOGGER;
	}
	else if (log_type == "SYSLOG_LOGGER") {
		m_logger_type = SYSLOG_LOGGER;
	}
	else {
		std::string error_str = "unknown logger type: " + log_type;
		error_str += "logger type property can only take STDOUT_LOGGER, FILE_LOGGER or SYSLOG_LOGGER as value.";
		throw internal_error(error_str);
	}

	std::string log_flags = logger_value.get("flags", "PLOG_NONE").asString();

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("|");
	tokenizer tokens(log_flags, sep);

	for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
		std::string flag = *tok_iter;
		boost::trim(flag);

		if (flag == "PLOG_NONE") {
			m_logger_flags |= PLOG_NONE;
		}
		else if (flag == "PLOG_INFO") {
			m_logger_flags |= PLOG_INFO;
		}
		else if (flag == "PLOG_DEBUG") {
			m_logger_flags |= PLOG_DEBUG;
		}
		else if (flag == "PLOG_WARNING") {
			m_logger_flags |= PLOG_WARNING;
		}
		else if (flag == "PLOG_ERROR") {
			m_logger_flags |= PLOG_ERROR;
		}
		else if (flag == "PLOG_TYPES") {
			m_logger_flags |= PLOG_TYPES;
		}
		else if (flag == "PLOG_TIME") {
			m_logger_flags |= PLOG_TIME;
		}
		else if (flag == "PLOG_ALL") {
			m_logger_flags |= PLOG_ALL;
		}
		else if (flag == "PLOG_BASIC") {
			m_logger_flags |= PLOG_BASIC;
		}
		else if (flag == "PLOG_INTRO") {
			m_logger_flags |= PLOG_INTRO;
		}
		else {
			std::string error_str = "unknown logger flag: " + flag;
			error_str += ", logger flag property can only take a combination of these values: ";
			error_str += "PLOG_NONE or PLOG_ALL, PLOG_BASIC, PLOG_INFO, PLOG_DEBUG, ";
			error_str += "PLOG_WARNING, PLOG_ERROR, PLOG_TYPES, PLOG_TIME, PLOG_INTRO";
			throw internal_error(error_str);
		}
	}

	if (m_logger_type == FILE_LOGGER) {
		m_logger_file_path  = logger_value.get("file", "").asString();
		boost::trim(m_logger_file_path);

		if (m_logger_file_path.empty()) {
			std::string error_str = "logger of type FILE_LOGGER must have non-empty \"file\" value.";
			throw internal_error(error_str);
		}
	}

	if (m_logger_type == SYSLOG_LOGGER) {
		m_logger_syslog_identity  = logger_value.get("identity", "").asString();
		boost::trim(m_logger_syslog_identity);

		if (m_logger_syslog_identity.empty()) {
			std::string error_str = "logger of type SYSLOG_LOGGER must have non-empty \"identity\" value.";
			throw internal_error(error_str);
		}
	}
}

void
configuration_t::parse_messages_cache_settings(const Json::Value& config_value) {
	const Json::Value cache_value = config_value["message_cache_t"];
	
	std::string message_cache_type_str = cache_value.get("type", "RAM_ONLY").asString();

	if (message_cache_type_str == "PERSISTENT") {
		m_message_cache_type = PERSISTENT;
	}
	else if (message_cache_type_str == "RAM_ONLY") {
		m_message_cache_type = RAM_ONLY;
	}
	else {
		std::string error_str = "unknown message cache type: " + message_cache_type_str;
		error_str += "message_cache_t \"type\" property can only take RAM_ONLY or PERSISTENT as value.";
		throw internal_error(error_str);
	}
}

void
configuration_t::parse_persistant_storage_settings(const Json::Value& config_value) {
	const Json::Value persistent_storage_value = config_value["persistent_storage"];

	m_eblob_path = persistent_storage_value.get("eblob_path", defaults_t::eblob_path).asString();
	m_eblob_blob_size = persistent_storage_value.get("blob_size", 0).asInt();
	m_eblob_blob_size *= 1024;

	if (m_eblob_blob_size == 0) {
		m_eblob_blob_size = defaults_t::eblob_blob_size;
	}

	m_eblob_sync_interval = persistent_storage_value.get("eblob_sync_interval", defaults_t::eblob_sync_interval).asInt();
}

void
configuration_t::parse_statistics_settings(const Json::Value& config_value) {
	const Json::Value statistics_value = config_value["statistics"];

	m_statistics_enabled = statistics_value.get("enabled", false).asBool();
	m_remote_statistics_enabled = statistics_value.get("remote_access", false).asBool();
	m_remote_statistics_port = (DT::port)statistics_value.get("remote_port", defaults_t::statistics_port).asUInt();
}

void
configuration_t::parse_services_settings(const Json::Value& config_value) {
	const Json::Value services_list = config_value["services"];

	if (!services_list.isObject() || !services_list.size()) {
		std::string error_str = "\"services\" section is malformed, it must have at least one service defined, ";
		error_str += "see documentation for more info.";
		throw internal_error(error_str);
	}

	Json::Value::Members services_names(services_list.getMemberNames());
	for (Json::Value::Members::iterator it = services_names.begin(); it != services_names.end(); ++it) {
	    std::string service_name(*it);
	    Json::Value service_data(services_list[service_name]);
	    
	    if (!service_data.isObject()) {
			std::string error_str = "\"service\" (with alias: " + service_name + ") is malformed, ";
			error_str += "see documentation for more info.";
			throw internal_error(error_str);
		}

	    service_info_t si;
		si.name = service_name;
		boost::trim(si.name);

	    if (si.name.empty()) {
	    	std::string error_str = "malformed \"service\" section found, alias must me non-empty string";
			throw internal_error(error_str);
	    }

	    si.description = service_data.get("description", "").asString();
	    boost::trim(si.description);

		si.app = service_data.get("app", "").asString();
		boost::trim(si.app);
		
		if (si.app.empty()) {
			std::string error_str = "malformed \"service\" " + si.name + " section found, field";
			error_str += "\"app\" must me non-empty string";
			throw internal_error(error_str);
		}

		// cocaine nodes autodiscovery
		const Json::Value autodiscovery = service_data["autodiscovery"];
		if (!autodiscovery.isObject()) {
			std::string error_str = "\"autodiscovery\" section for service " + service_name + " is malformed, ";
			error_str += "see documentation for more info.";
			throw internal_error(error_str);
		}

		si.hosts_source = autodiscovery.get("source", "").asString();
		boost::trim(si.hosts_source);

		if (si.hosts_source.empty()) {
			std::string error_str = "malformed \"service\" " + si.name + " section found, field";
			error_str += "\"source\" must me non-empty string";
			throw internal_error(error_str);
		}

		std::string autodiscovery_type_str = autodiscovery.get("type", "").asString();

		if (autodiscovery_type_str == "FILE") {
			si.discovery_type = AT_FILE;
		}
		else if (autodiscovery_type_str == "HTTP") {
			si.discovery_type = AT_HTTP;
		}
		else if (autodiscovery_type_str == "MULTICAST") {
			si.discovery_type = AT_MULTICAST;
		}
		else {
			std::string error_str = "\"autodiscovery\" section for service " + service_name;
			error_str += " has malformed field \"type\", which can only take values FILE, HTTP, MULTICAST.";
			throw internal_error(error_str);
		}

		// check for duplicate services
		std::map<std::string, service_info_t>::iterator it = m_services_list.begin();
		for (;it != m_services_list.end(); ++it) {
			if (it->second.name == si.name) {
				throw internal_error("duplicate service with name " + si.name + " was found in config!");
			}
		}

		m_services_list[si.name] = si;
	}
}

void
configuration_t::load(const std::string& path) {
	boost::mutex::scoped_lock lock(m_mutex);

	m_path = path;

	std::ifstream file(path.c_str(), std::ifstream::in);
	
	if (!file.is_open()) {
		throw internal_error("config file: " + path + " failed to open at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	std::string config_data;
	std::string line;
	while (std::getline(file, line)) {
		// strip comments
		boost::trim(line);
		if (line.substr(0, 2) == "//") {
			continue;
		}

		// append to config otherwise
		config_data += line;
	}
	
	file.close();

	Json::Value root;
	Json::Reader reader;
	bool parsing_successful = reader.parse(config_data, root);
		
	if (!parsing_successful) {
		throw internal_error("config file: " + path + " could not be parsed at: " + std::string(BOOST_CURRENT_FUNCTION));
	}
	
	try {
		parse_basic_settings(root);
		parse_logger_settings(root);
		parse_messages_cache_settings(root);
		parse_persistant_storage_settings(root);
		parse_services_settings(root);
		//parse_statistics_settings(config_value);
	}
	catch (const std::exception& ex) {
		std::string error_msg = "config file: " + path + " could not be parsed. details: ";
		error_msg += ex.what();
		throw internal_error(error_msg);
	}
}

void
configuration_t::parse_basic_settings(const Json::Value& config_value) {
	int file_version = config_value.get("version", 0).asUInt();

	if (file_version != current_config_version) {
		throw internal_error("Unsupported config version: %d, current version: %d", file_version, current_config_version);
	}

	// parse message_deadline
	const Json::Value deadline_value = config_value.get("default_message_deadline",
															   static_cast<int>(defaults_t::default_message_deadline));

	m_default_message_deadline = static_cast<unsigned long long>(deadline_value.asInt());
}

const std::string&
configuration_t::config_path() const {
	return m_path;
}

unsigned int
configuration_t::config_version() const {
	return current_config_version;
}

unsigned long long
configuration_t::default_message_deadline() const {
	return m_default_message_deadline;
}

enum e_message_cache_type
configuration_t::message_cache_type() const {
	return m_message_cache_type;
}

enum e_logger_type
configuration_t::logger_type() const {
	return m_logger_type;
}

unsigned int
configuration_t::logger_flags() const {
	return m_logger_flags;
}

const std::string&
configuration_t::logger_file_path() const {
	return m_logger_file_path;
}

const std::string&
configuration_t::logger_syslog_identity() const {
	return m_logger_syslog_identity;
}

std::string
configuration_t::eblob_path() const {
	return m_eblob_path;
}

int64_t
configuration_t::eblob_blob_size() const {
	return m_eblob_blob_size;
}

int
configuration_t::eblob_sync_interval() const {
	return m_eblob_sync_interval;
}

bool
configuration_t::is_statistics_enabled() const {
	return m_statistics_enabled;
}

bool
configuration_t::is_remote_statistics_enabled() const {
	return m_remote_statistics_enabled;
}

DT::port
configuration_t::remote_statistics_port() const {
	return m_remote_statistics_port;
}

const std::map<std::string, service_info_t>&
configuration_t::services_list() const {
	return m_services_list;
}

bool
configuration_t::service_info_by_name(const std::string& name, service_info_t& info) const {
	std::map<std::string, service_info_t>::const_iterator it = m_services_list.find(name);
	
	if (it != m_services_list.end()) {
		info = it->second;
		return true;
	}

	return false;
}

bool
configuration_t::service_info_by_name(const std::string& name) const {
	std::map<std::string, service_info_t>::const_iterator it = m_services_list.find(name);

	if (it != m_services_list.end()) {
		return true;
	}

	return false;
}

std::ostream& operator << (std::ostream& out, configuration_t& c) {
	out << "---------- config path: " << c.m_path << " ----------\n";

	// basic
	out << "basic settings\n";
	out << "\tconfig version: " << configuration_t::current_config_version << "\n";
	out << "\tdefault message deadline: " << c.m_default_message_deadline << "\n";
	
	// logger
	out << "\nlogger\n";
	switch (c.m_logger_type) {
		case STDOUT_LOGGER:
			out << "\ttype: STDOUT_LOGGER" << "\n";
			break;
		case FILE_LOGGER:
			out << "\ttype: FILE_LOGGER" << "\n";
			out << "\tfile path: " << c.m_logger_file_path << "\n";
			break;
		case SYSLOG_LOGGER:
			out << "\ttype: SYSLOG_LOGGER" << "\n";
			out << "\tsyslog identity: " << c.m_logger_syslog_identity << "\n\n";
			break;
	}
	
	switch (c.m_logger_flags) {
		case PLOG_NONE:
			out << "\tflags: PLOG_NONE" << "\n";
			break;
		default:
			out << "\tflags: ";
			if ((c.m_logger_flags & PLOG_INFO) == PLOG_INFO) { out << "PLOG_INFO "; }
			if ((c.m_logger_flags & PLOG_DEBUG) == PLOG_DEBUG) { out << "PLOG_DEBUG "; }
			if ((c.m_logger_flags & PLOG_WARNING) == PLOG_WARNING) { out << "PLOG_WARNING "; }
			if ((c.m_logger_flags & PLOG_ERROR) == PLOG_ERROR) { out << "PLOG_ERROR "; }
			if ((c.m_logger_flags & PLOG_TYPES) == PLOG_TYPES) { out << "PLOG_TYPES "; }
			if ((c.m_logger_flags & PLOG_TIME) == PLOG_TIME) { out << "PLOG_TIME "; }
			if ((c.m_logger_flags & PLOG_INTRO) == PLOG_INTRO) { out << "PLOG_INTRO "; }
			out << "\n";
			break;
	}

	out << "\n";

 	// message cache
 	out << "message cache\n";

 	if (c.m_message_cache_type == RAM_ONLY) {
 		out << "\ttype: RAM_ONLY\n\n";
 	}
 	else if (c.m_message_cache_type == PERSISTENT) {
 		out << "\ttype: PERSISTENT\n\n";

 		// persistant storage
 		out << "persistant storage\n";
		out << "\teblob path: " << c.m_eblob_path << "\n";
 		out << "\teblob sync interval: " << c.m_eblob_sync_interval << "\n\n";
 	}

	// services
	out << "services: ";
	const std::map<std::string, service_info_t>& sl = c.m_services_list;
	
	std::map<std::string, service_info_t>::const_iterator it = sl.begin();
	for (; it != sl.end(); ++it) {
		out << "\n\talias: " << it->second.name << "\n";
		out << "\tdescription: " << it->second.description << "\n";
		out << "\tapp: " << it->second.app << "\n";
		out << "\thosts source: " << it->second.hosts_source << "\n";

		switch (it->second.discovery_type) {
			case AT_MULTICAST:
				out << "\tautodiscovery type: multicast" << "\n";
				break;
			case AT_HTTP:
				out << "\tautodiscovery type: http" << "\n";
				break;
			case AT_FILE:
				out << "\tautodiscovery type: file" << "\n";
				break;
		}
	}

 	/*
	// statistics
	out << "statistics\n";
	if (m_statistics_enabled == true) {
		out << "\tenabled: true" << "\n";
	}
	else {
		out << "\tenabled: false" << "\n";
	}

	if (m_remote_statistics_enabled == true) {
		out << "\tremote enabled: true" << "\n";
	}
	else {
		out << "\tremote enabled: false" << "\n";
	}

	out << "\tremote port: " << m_remote_statistics_port << "\n\n";
	*/

	return out;
}

} // namespace dealer
} // namespace cocaine
