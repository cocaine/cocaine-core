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

configuration::configuration() :
	default_message_deadline_m(defaults::default_message_deadline),
	message_cache_type_m(defaults::message_cache_type),
	logger_type_m(defaults::logger_type),
	logger_flags_m(defaults::logger_flags),
	eblob_path_m(defaults::eblob_path),
	eblob_blob_size_m(defaults::eblob_blob_size),
	eblob_sync_interval_(defaults::eblob_sync_interval),
	is_statistics_enabled_m(false),
	is_remote_statistics_enabled_m(false),
	remote_statistics_port_m(defaults::statistics_port)
{
	
}

configuration::configuration(const std::string& path) :
	path_m(path),
	default_message_deadline_m(defaults::default_message_deadline),
	message_cache_type_m(defaults::message_cache_type),
	logger_type_m(defaults::logger_type),
	logger_flags_m(defaults::logger_flags),
	eblob_path_m(defaults::eblob_path),
	eblob_blob_size_m(defaults::eblob_blob_size),
	eblob_sync_interval_(defaults::eblob_sync_interval),
	is_statistics_enabled_m(false),
	is_remote_statistics_enabled_m(false),
	remote_statistics_port_m(defaults::statistics_port)
{
	load(path);
}

configuration::~configuration() {
	
}

void
configuration::parse_logger_settings(const Json::Value& config_value) {
	const Json::Value logger_value = config_value["logger"];
	std::string log_type = logger_value.get("type", "STDOUT_LOGGER").asString();

	if (log_type == "STDOUT_LOGGER") {
		logger_type_m = STDOUT_LOGGER;
	}
	else if (log_type == "FILE_LOGGER") {
		logger_type_m = FILE_LOGGER;
	}
	else if (log_type == "SYSLOG_LOGGER") {
		logger_type_m = SYSLOG_LOGGER;
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
			logger_flags_m |= PLOG_NONE;
		}
		else if (flag == "PLOG_INFO") {
			logger_flags_m |= PLOG_INFO;
		}
		else if (flag == "PLOG_DEBUG") {
			logger_flags_m |= PLOG_DEBUG;
		}
		else if (flag == "PLOG_WARNING") {
			logger_flags_m |= PLOG_WARNING;
		}
		else if (flag == "PLOG_ERROR") {
			logger_flags_m |= PLOG_ERROR;
		}
		else if (flag == "PLOG_TYPES") {
			logger_flags_m |= PLOG_TYPES;
		}
		else if (flag == "PLOG_TIME") {
			logger_flags_m |= PLOG_TIME;
		}
		else if (flag == "PLOG_ALL") {
			logger_flags_m |= PLOG_ALL;
		}
		else if (flag == "PLOG_BASIC") {
			logger_flags_m |= PLOG_BASIC;
		}
		else if (flag == "PLOG_INTRO") {
			logger_flags_m |= PLOG_INTRO;
		}
		else {
			std::string error_str = "unknown logger flag: " + flag;
			error_str += ", logger flag property can only take a combination of these values: ";
			error_str += "PLOG_NONE or PLOG_ALL, PLOG_BASIC, PLOG_INFO, PLOG_DEBUG, ";
			error_str += "PLOG_WARNING, PLOG_ERROR, PLOG_TYPES, PLOG_TIME, PLOG_INTRO";
			throw internal_error(error_str);
		}
	}

	if (logger_type_m == FILE_LOGGER) {
		logger_file_path_m  = logger_value.get("file", "").asString();
		boost::trim(logger_file_path_m);

		if (logger_file_path_m.empty()) {
			std::string error_str = "logger of type FILE_LOGGER must have non-empty \"file\" value.";
			throw internal_error(error_str);
		}
	}

	if (logger_type_m == SYSLOG_LOGGER) {
		logger_syslog_identity_m  = logger_value.get("identity", "").asString();
		boost::trim(logger_syslog_identity_m);

		if (logger_syslog_identity_m.empty()) {
			std::string error_str = "logger of type SYSLOG_LOGGER must have non-empty \"identity\" value.";
			throw internal_error(error_str);
		}
	}
}

void
configuration::parse_messages_cache_settings(const Json::Value& config_value) {
	const Json::Value cache_value = config_value["message_cache"];
	
	std::string message_cache_type_str = cache_value.get("type", "RAM_ONLY").asString();

	if (message_cache_type_str == "PERSISTENT") {
		message_cache_type_m = PERSISTENT;
	}
	else if (message_cache_type_str == "RAM_ONLY") {
		message_cache_type_m = RAM_ONLY;
	}
	else {
		std::string error_str = "unknown message cache type: " + message_cache_type_str;
		error_str += "message_cache \"type\" property can only take RAM_ONLY or PERSISTENT as value.";
		throw internal_error(error_str);
	}
}

void
configuration::parse_persistant_storage_settings(const Json::Value& config_value) {
	const Json::Value persistent_storage_value = config_value["persistent_storage"];

	eblob_path_m = persistent_storage_value.get("eblob_path", defaults::eblob_path).asString();
	eblob_blob_size_m = persistent_storage_value.get("blob_size", 0).asInt();
	eblob_blob_size_m *= 1024;

	if (eblob_blob_size_m == 0) {
		eblob_blob_size_m = defaults::eblob_blob_size;
	}

	eblob_sync_interval_ = persistent_storage_value.get("eblob_sync_interval", defaults::eblob_sync_interval).asInt();
}

void
configuration::parse_statistics_settings(const Json::Value& config_value) {
	const Json::Value statistics_value = config_value["statistics"];

	is_statistics_enabled_m = statistics_value.get("enabled", false).asBool();
	is_remote_statistics_enabled_m = statistics_value.get("remote_access", false).asBool();
	remote_statistics_port_m = (DT::port)statistics_value.get("remote_port", defaults::statistics_port).asUInt();
}

void
configuration::parse_services_settings(const Json::Value& config_value) {
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
		std::map<std::string, service_info_t>::iterator it = services_list_m.begin();
		for (;it != services_list_m.end(); ++it) {
			if (it->second.name == si.name) {
				throw internal_error("duplicate service with name " + si.name + " was found in config!");
			}
		}

		services_list_m[si.name] = si;
	}
}

void
configuration::load(const std::string& path) {
	boost::mutex::scoped_lock lock(mutex_m);

	path_m = path;

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
configuration::parse_basic_settings(const Json::Value& config_value) {
	int file_version = config_value.get("version", 0).asUInt();

	if (file_version != current_config_version) {
		throw internal_error("Unsupported config version: %d, current version: %d", file_version, current_config_version);
	}

	// parse message_deadline
	const Json::Value deadline_value = config_value.get("default_message_deadline",
															   static_cast<int>(defaults::default_message_deadline));

	default_message_deadline_m = static_cast<unsigned long long>(deadline_value.asInt());
}

const std::string&
configuration::config_path() const {
	return path_m;
}

unsigned int
configuration::config_version() const {
	return current_config_version;
}

unsigned long long
configuration::default_message_deadline() const {
	return default_message_deadline_m;
}

enum e_message_cache_type
configuration::message_cache_type() const {
	return message_cache_type_m;
}

enum e_logger_type
configuration::logger_type() const {
	return logger_type_m;
}

unsigned int
configuration::logger_flags() const {
	return logger_flags_m;
}

const std::string&
configuration::logger_file_path() const {
	return logger_file_path_m;
}

const std::string&
configuration::logger_syslog_identity() const {
	return logger_syslog_identity_m;
}

std::string
configuration::eblob_path() const {
	return eblob_path_m;
}

int64_t
configuration::eblob_blob_size() const {
	return eblob_blob_size_m;
}

int
configuration::eblob_sync_interval() const {
	return eblob_sync_interval_;
}

bool
configuration::is_statistics_enabled() const {
	return is_statistics_enabled_m;
}

bool
configuration::is_remote_statistics_enabled() const {
	return is_remote_statistics_enabled_m;
}

DT::port
configuration::remote_statistics_port() const {
	return remote_statistics_port_m;
}

const std::map<std::string, service_info_t>&
configuration::services_list() const {
	return services_list_m;
}

bool
configuration::service_info_by_name(const std::string& name, service_info_t& info) const {
	std::map<std::string, service_info_t>::const_iterator it = services_list_m.find(name);
	
	if (it != services_list_m.end()) {
		info = it->second;
		return true;
	}

	return false;
}

bool
configuration::service_info_by_name(const std::string& name) const {
	std::map<std::string, service_info_t>::const_iterator it = services_list_m.find(name);

	if (it != services_list_m.end()) {
		return true;
	}

	return false;
}

std::ostream& operator << (std::ostream& out, configuration& c) {
	out << "---------- config path: " << c.path_m << " ----------\n";

	// basic
	out << "basic settings\n";
	out << "\tconfig version: " << configuration::current_config_version << "\n";
	out << "\tdefault message deadline: " << c.default_message_deadline_m << "\n";
	
	// logger
	out << "\nlogger\n";
	switch (c.logger_type_m) {
		case STDOUT_LOGGER:
			out << "\ttype: STDOUT_LOGGER" << "\n";
			break;
		case FILE_LOGGER:
			out << "\ttype: FILE_LOGGER" << "\n";
			out << "\tfile path: " << c.logger_file_path_m << "\n";
			break;
		case SYSLOG_LOGGER:
			out << "\ttype: SYSLOG_LOGGER" << "\n";
			out << "\tsyslog identity: " << c.logger_syslog_identity_m << "\n\n";
			break;
	}
	
	switch (c.logger_flags_m) {
		case PLOG_NONE:
			out << "\tflags: PLOG_NONE" << "\n";
			break;
		default:
			out << "\tflags: ";
			if ((c.logger_flags_m & PLOG_INFO) == PLOG_INFO) { out << "PLOG_INFO "; }
			if ((c.logger_flags_m & PLOG_DEBUG) == PLOG_DEBUG) { out << "PLOG_DEBUG "; }
			if ((c.logger_flags_m & PLOG_WARNING) == PLOG_WARNING) { out << "PLOG_WARNING "; }
			if ((c.logger_flags_m & PLOG_ERROR) == PLOG_ERROR) { out << "PLOG_ERROR "; }
			if ((c.logger_flags_m & PLOG_TYPES) == PLOG_TYPES) { out << "PLOG_TYPES "; }
			if ((c.logger_flags_m & PLOG_TIME) == PLOG_TIME) { out << "PLOG_TIME "; }
			if ((c.logger_flags_m & PLOG_INTRO) == PLOG_INTRO) { out << "PLOG_INTRO "; }
			out << "\n";
			break;
	}

	out << "\n";

 	// message cache
 	out << "message cache\n";

 	if (c.message_cache_type_m == RAM_ONLY) {
 		out << "\ttype: RAM_ONLY\n\n";
 	}
 	else if (c.message_cache_type_m == PERSISTENT) {
 		out << "\ttype: PERSISTENT\n\n";

 		// persistant storage
 		out << "persistant storage\n";
		out << "\teblob path: " << c.eblob_path_m << "\n";
 		out << "\teblob sync interval: " << c.eblob_sync_interval_ << "\n\n";
 	}

	// services
	out << "services: ";
	const std::map<std::string, service_info_t>& sl = c.services_list_m;
	
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
	if (is_statistics_enabled_m == true) {
		out << "\tenabled: true" << "\n";
	}
	else {
		out << "\tenabled: false" << "\n";
	}

	if (is_remote_statistics_enabled_m == true) {
		out << "\tremote enabled: true" << "\n";
	}
	else {
		out << "\tremote enabled: false" << "\n";
	}

	out << "\tremote port: " << remote_statistics_port_m << "\n\n";
	*/

	return out;
}

} // namespace dealer
} // namespace cocaine
