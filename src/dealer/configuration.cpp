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
	default_message_deadline_(defaults::default_message_deadline),
	message_cache_type_(defaults::message_cache_type),
	logger_type_(defaults::logger_type),
	logger_flags_(defaults::logger_flags),
	eblob_path_(defaults::eblob_path),
	eblob_blob_size_(defaults::eblob_blob_size),
	eblob_sync_interval_(defaults::eblob_sync_interval),
	is_statistics_enabled_(false),
	is_remote_statistics_enabled_(false),
	remote_statistics_port_(defaults::statistics_port)
{
	
}

configuration::configuration(const std::string& path) :
	path_(path),
	default_message_deadline_(defaults::default_message_deadline),
	message_cache_type_(defaults::message_cache_type),
	logger_type_(defaults::logger_type),
	logger_flags_(defaults::logger_flags),
	eblob_path_(defaults::eblob_path),
	eblob_blob_size_(defaults::eblob_blob_size),
	eblob_sync_interval_(defaults::eblob_sync_interval),
	is_statistics_enabled_(false),
	is_remote_statistics_enabled_(false),
	remote_statistics_port_(defaults::statistics_port)
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
		logger_type_ = STDOUT_LOGGER;
	}
	else if (log_type == "FILE_LOGGER") {
		logger_type_ = FILE_LOGGER;
	}
	else if (log_type == "SYSLOG_LOGGER") {
		logger_type_ = SYSLOG_LOGGER;
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
			logger_flags_ |= PLOG_NONE;
		}
		else if (flag == "PLOG_INFO") {
			logger_flags_ |= PLOG_INFO;
		}
		else if (flag == "PLOG_DEBUG") {
			logger_flags_ |= PLOG_DEBUG;
		}
		else if (flag == "PLOG_WARNING") {
			logger_flags_ |= PLOG_WARNING;
		}
		else if (flag == "PLOG_ERROR") {
			logger_flags_ |= PLOG_ERROR;
		}
		else if (flag == "PLOG_TYPES") {
			logger_flags_ |= PLOG_TYPES;
		}
		else if (flag == "PLOG_TIME") {
			logger_flags_ |= PLOG_TIME;
		}
		else if (flag == "PLOG_ALL") {
			logger_flags_ |= PLOG_ALL;
		}
		else if (flag == "PLOG_BASIC") {
			logger_flags_ |= PLOG_BASIC;
		}
		else if (flag == "PLOG_INTRO") {
			logger_flags_ |= PLOG_INTRO;
		}
		else {
			std::string error_str = "unknown logger flag: " + flag;
			error_str += ", logger flag property can only take a combination of these values: ";
			error_str += "PLOG_NONE or PLOG_ALL, PLOG_BASIC, PLOG_INFO, PLOG_DEBUG, ";
			error_str += "PLOG_WARNING, PLOG_ERROR, PLOG_TYPES, PLOG_TIME, PLOG_INTRO";
			throw internal_error(error_str);
		}
	}

	if (logger_type_ == FILE_LOGGER) {
		logger_file_path_  = logger_value.get("file", "").asString();
		boost::trim(logger_file_path_);

		if (logger_file_path_.empty()) {
			std::string error_str = "logger of type FILE_LOGGER must have non-empty \"file\" value.";
			throw internal_error(error_str);
		}
	}

	if (logger_type_ == SYSLOG_LOGGER) {
		logger_syslog_identity_  = logger_value.get("identity", "").asString();
		boost::trim(logger_syslog_identity_);

		if (logger_syslog_identity_.empty()) {
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
		message_cache_type_ = PERSISTENT;
	}
	else if (message_cache_type_str == "RAM_ONLY") {
		message_cache_type_ = RAM_ONLY;
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

	eblob_path_ = persistent_storage_value.get("eblob_path", defaults::eblob_path).asString();
	eblob_blob_size_ = persistent_storage_value.get("blob_size", 0).asInt();
	eblob_blob_size_ *= 1024;

	if (eblob_blob_size_ == 0) {
		eblob_blob_size_ = defaults::eblob_blob_size;
	}

	eblob_sync_interval_ = persistent_storage_value.get("eblob_sync_interval", defaults::eblob_sync_interval).asInt();
}

void
configuration::parse_statistics_settings(const Json::Value& config_value) {
	const Json::Value statistics_value = config_value["statistics"];

	is_statistics_enabled_ = statistics_value.get("enabled", false).asBool();
	is_remote_statistics_enabled_ = statistics_value.get("remote_access", false).asBool();
	remote_statistics_port_ = (DT::port)statistics_value.get("remote_port", defaults::statistics_port).asUInt();
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
		si.name_ = service_name;
		boost::trim(si.name_);

	    if (si.name_.empty()) {
	    	std::string error_str = "malformed \"service\" section found, alias must me non-empty string";
			throw internal_error(error_str);
	    }

	    si.description_ = service_data.get("description", "").asString();
	    boost::trim(si.description_);

		si.app_ = service_data.get("app", "").asString();
		boost::trim(si.app_);
		
		if (si.app_.empty()) {
			std::string error_str = "malformed \"service\" " + si.name_ + " section found, field";
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

		si.hosts_source_ = autodiscovery.get("source", "").asString();
		boost::trim(si.hosts_source_);

		if (si.hosts_source_.empty()) {
			std::string error_str = "malformed \"service\" " + si.name_ + " section found, field";
			error_str += "\"source\" must me non-empty string";
			throw internal_error(error_str);
		}

		std::string autodiscovery_type_str = autodiscovery.get("type", "").asString();

		if (autodiscovery_type_str == "FILE") {
			si.discovery_type_ = AT_FILE;
		}
		else if (autodiscovery_type_str == "HTTP") {
			si.discovery_type_ = AT_HTTP;
		}
		else if (autodiscovery_type_str == "MULTICAST") {
			si.discovery_type_ = AT_MULTICAST;
		}
		else {
			std::string error_str = "\"autodiscovery\" section for service " + service_name;
			error_str += " has malformed field \"type\", which can only take values FILE, HTTP, MULTICAST.";
			throw internal_error(error_str);
		}

		// check for duplicate services
		std::map<std::string, service_info_t>::iterator it = services_list_.begin();
		for (;it != services_list_.end(); ++it) {
			if (it->second.name_ == si.name_) {
				throw internal_error("duplicate service with name " + si.name_ + " was found in config!");
			}
		}

		services_list_[si.name_] = si;
	}
}

void
configuration::load(const std::string& path) {
	boost::mutex::scoped_lock lock(mutex_);

	path_ = path;

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

	default_message_deadline_ = static_cast<unsigned long long>(deadline_value.asInt());
}

const std::string&
configuration::config_path() const {
	return path_;
}

unsigned int
configuration::config_version() const {
	return current_config_version;
}

unsigned long long
configuration::default_message_deadline() const {
	return default_message_deadline_;
}

enum e_message_cache_type
configuration::message_cache_type() const {
	return message_cache_type_;
}

enum e_logger_type
configuration::logger_type() const {
	return logger_type_;
}

unsigned int
configuration::logger_flags() const {
	return logger_flags_;
}

const std::string&
configuration::logger_file_path() const {
	return logger_file_path_;
}

const std::string&
configuration::logger_syslog_identity() const {
	return logger_syslog_identity_;
}

std::string
configuration::eblob_path() const {
	return eblob_path_;
}

int64_t
configuration::eblob_blob_size() const {
	return eblob_blob_size_;
}

int
configuration::eblob_sync_interval() const {
	return eblob_sync_interval_;
}

bool
configuration::is_statistics_enabled() const {
	return is_statistics_enabled_;
}

bool
configuration::is_remote_statistics_enabled() const {
	return is_remote_statistics_enabled_;
}

DT::port
configuration::remote_statistics_port() const {
	return remote_statistics_port_;
}

const std::map<std::string, service_info_t>&
configuration::services_list() const {
	return services_list_;
}

bool
configuration::service_info_by_name(const std::string& name, service_info_t& info) const {
	std::map<std::string, service_info_t>::const_iterator it = services_list_.find(name);
	
	if (it != services_list_.end()) {
		info = it->second;
		return true;
	}

	return false;
}

bool
configuration::service_info_by_name(const std::string& name) const {
	std::map<std::string, service_info_t>::const_iterator it = services_list_.find(name);

	if (it != services_list_.end()) {
		return true;
	}

	return false;
}

std::ostream& operator << (std::ostream& out, configuration& c) {
	out << "---------- config path: " << c.path_ << " ----------\n";

	// basic
	out << "basic settings\n";
	out << "\tconfig version: " << configuration::current_config_version << "\n";
	out << "\tdefault message deadline: " << c.default_message_deadline_ << "\n";
	
	// logger
	out << "\nlogger\n";
	switch (c.logger_type_) {
		case STDOUT_LOGGER:
			out << "\ttype: STDOUT_LOGGER" << "\n";
			break;
		case FILE_LOGGER:
			out << "\ttype: FILE_LOGGER" << "\n";
			out << "\tfile path: " << c.logger_file_path_ << "\n";
			break;
		case SYSLOG_LOGGER:
			out << "\ttype: SYSLOG_LOGGER" << "\n";
			out << "\tsyslog identity: " << c.logger_syslog_identity_ << "\n\n";
			break;
	}
	
	switch (c.logger_flags_) {
		case PLOG_NONE:
			out << "\tflags: PLOG_NONE" << "\n";
			break;
		default:
			out << "\tflags: ";
			if ((c.logger_flags_ & PLOG_INFO) == PLOG_INFO) { out << "PLOG_INFO "; }
			if ((c.logger_flags_ & PLOG_DEBUG) == PLOG_DEBUG) { out << "PLOG_DEBUG "; }
			if ((c.logger_flags_ & PLOG_WARNING) == PLOG_WARNING) { out << "PLOG_WARNING "; }
			if ((c.logger_flags_ & PLOG_ERROR) == PLOG_ERROR) { out << "PLOG_ERROR "; }
			if ((c.logger_flags_ & PLOG_TYPES) == PLOG_TYPES) { out << "PLOG_TYPES "; }
			if ((c.logger_flags_ & PLOG_TIME) == PLOG_TIME) { out << "PLOG_TIME "; }
			if ((c.logger_flags_ & PLOG_INTRO) == PLOG_INTRO) { out << "PLOG_INTRO "; }
			out << "\n";
			break;
	}

	out << "\n";

 	// message cache
 	out << "message cache\n";

 	if (c.message_cache_type_ == RAM_ONLY) {
 		out << "\ttype: RAM_ONLY\n\n";
 	}
 	else if (c.message_cache_type_ == PERSISTENT) {
 		out << "\ttype: PERSISTENT\n\n";

 		// persistant storage
 		out << "persistant storage\n";
		out << "\teblob path: " << c.eblob_path_ << "\n";
 		out << "\teblob sync interval: " << c.eblob_sync_interval_ << "\n\n";
 	}

	// services
	out << "services: ";
	const std::map<std::string, service_info_t>& sl = c.services_list_;
	
	std::map<std::string, service_info_t>::const_iterator it = sl.begin();
	for (; it != sl.end(); ++it) {
		out << "\n\talias: " << it->second.name_ << "\n";
		out << "\tdescription: " << it->second.description_ << "\n";
		out << "\tapp: " << it->second.app_ << "\n";
		out << "\thosts source: " << it->second.hosts_source_ << "\n";

		switch (it->second.discovery_type_) {
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
	if (is_statistics_enabled_ == true) {
		out << "\tenabled: true" << "\n";
	}
	else {
		out << "\tenabled: false" << "\n";
	}

	if (is_remote_statistics_enabled_ == true) {
		out << "\tremote enabled: true" << "\n";
	}
	else {
		out << "\tremote enabled: false" << "\n";
	}

	out << "\tremote port: " << remote_statistics_port_ << "\n\n";
	*/

	return out;
}

} // namespace dealer
} // namespace cocaine
