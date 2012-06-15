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

#ifndef _COCAINE_DEALER_SMART_LOGGER_HPP_INCLUDED_
#define _COCAINE_DEALER_SMART_LOGGER_HPP_INCLUDED_

#include <fstream>
#include <iostream>

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>

#include <boost/current_function.hpp>

#include "cocaine/dealer/utils/error.hpp"

namespace cocaine {
namespace dealer {

#define PLOG_NONE		(0x0)
#define PLOG_INFO		(0x1 << 0)
#define PLOG_DEBUG		(0x1 << 1)
#define PLOG_WARNING	(0x1 << 2)
#define PLOG_ERROR		(0x1 << 3)
#define PLOG_TYPES 		(0x1 << 4)
#define PLOG_TIME		(0x1 << 5)
#define PLOG_INTRO		(0x1 << 6)
#define PLOG_BASIC		(PLOG_TYPES | PLOG_TIME | PLOG_INFO)
#define PLOG_ALL		(PLOG_TYPES | PLOG_TIME | PLOG_INFO | PLOG_DEBUG | PLOG_WARNING | PLOG_ERROR)

enum e_logger_type {
	STDOUT_LOGGER = 1,
	FILE_LOGGER,
	SYSLOG_LOGGER
};

// base logger functionality class
class base_logger_t {
public:
	base_logger_t() :
		flags_m(PLOG_ALL) {}

	base_logger_t(unsigned int flags) :
		flags_m(flags)
	{
		
		if ((flags_m & PLOG_INTRO) == PLOG_INTRO) {
			create_first_message();
		}
	}
	
	std::string get_message_prefix(unsigned int message_type) {
		std::string prefix;

		time_t now = time(NULL);
		if ((flags_m & PLOG_TIME) == PLOG_TIME) {
			struct tm* ptm = localtime(&now);
			char buffer[32];
			strftime (buffer, 32, "%H:%M:%S", ptm);
			prefix += "[";
			prefix += buffer;
			prefix += "]";
		}

		if ((flags_m & PLOG_TYPES) == PLOG_TYPES) {
			if ((message_type & PLOG_INFO) == PLOG_INFO) {
				prefix += "[INFO]    ";
			}
			else if ((message_type & PLOG_DEBUG) == PLOG_DEBUG) {
				prefix += "[DEBUG]   ";
			}
			else if ((message_type & PLOG_WARNING) == PLOG_WARNING) {
				prefix += "[WARNING] ";
			}
			else if ((message_type & PLOG_ERROR) == PLOG_ERROR) {
				prefix += "[ERROR]   ";
			}		
		}
		else {
			if (!prefix.empty()) {
				prefix += "    ";
			}
		}
		
		return prefix;
	}

	void log(const std::string& message, ...) {
		if ((flags_m & PLOG_INFO) != PLOG_INFO) {
			return;
		}

		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message.c_str(), vl);
		va_end(vl);

		internal_log(PLOG_INFO, std::string(buff));
	}

	void log(unsigned int type, const std::string& message, ...) {
		if ((flags_m & type) != type) {
			return;
		}

		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message.c_str(), vl);
		va_end(vl);

		internal_log(type, std::string(buff));
	}

	unsigned int flags() const { return flags_m; };

private:
	virtual void internal_log(__attribute__ ((unused)) unsigned int type, 
							  __attribute__ ((unused)) const std::string& message) {};

	void create_first_message() {
		if (flags_m == PLOG_NONE) {
			return;
		}

		time_t rawtime;
		struct tm* timeinfo;
		unsigned int buflen = 80;
		char buffer[buflen];

		memset(buffer, 0, sizeof(buffer));
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		strftime(buffer, buflen, "%c", timeinfo);

		first_message_m = "---------------------- new log started at ";
		first_message_m += std::string(buffer) + " ----------------------\n";	
	}

protected:
	unsigned int flags_m;
	std::string first_message_m;
};

// empty logger class
class empty_logger_t : public base_logger_t {
public:	
	empty_logger_t() :
		base_logger_t(PLOG_NONE) {}
};

// stdout logger class
class stdout_logger_t : public base_logger_t {
public:
	stdout_logger_t() {}

	stdout_logger_t(unsigned int flags) :
		base_logger_t(flags)
	{
		if ((flags_m & PLOG_INTRO) == PLOG_INTRO) {
			std::cout << first_message_m << std::flush;
		}
	}
	
private:
	void internal_log(unsigned int type, const std::string& message) {
		std::cout << get_message_prefix(type) << message << "\n" << std::flush;
	}
};

// file logger class
class file_logger_t : public base_logger_t {
public:
	file_logger_t() {}

	file_logger_t(unsigned int flags) :
		base_logger_t(flags) {}

	file_logger_t(unsigned int flags, const std::string& file_path) :
		base_logger_t(flags)
	{
		init(file_path);
	}
	
	void init(const std::string& file_path) {				
		file_path_m = file_path;

		try {
			file_m.open(file_path_m.c_str(), std::fstream::out | std::fstream::app);
		}
		catch (const std::exception& ex) {
			file_m.close();

			std::string error_msg = "logger creation failed. unable to open file: ";
			error_msg += file_path_m;
			error_msg += " for FILE_LOGGER";
			throw internal_error(error_msg + ", details: " + ex.what());
		}

		if (!file_m.is_open()) {
			file_m.close();

			std::string error_msg = "logger creation failed. unable to open file: ";
			error_msg += file_path_m;
			error_msg += " for FILE_LOGGER";
			throw internal_error(error_msg);
		}

		if ((flags_m & PLOG_INTRO) == PLOG_INTRO) {
			file_m << first_message_m;
		}
	}
	
	~file_logger_t() {
		file_m.close();
	}
	
private:
	void internal_log(unsigned int type, const std::string& message) {		
		if (!file_m.is_open()) {
			file_m.close();

			std::string error_msg = "writing to log failed. file: ";
			error_msg += file_path_m;
			error_msg += " is closed for FILE_LOGGER";
			throw internal_error(error_msg);
		}

		file_m << get_message_prefix(type) << message << "\n" << std::flush;
	}

	std::ofstream file_m;
	std::string file_path_m;
};

// syslog logger class
class syslog_logger_t : public base_logger_t {
public:
	syslog_logger_t() {}

	syslog_logger_t(unsigned int flags) :
		base_logger_t(flags) {}
	
	syslog_logger_t(unsigned int flags, const std::string& syslog_identity) :
		base_logger_t(flags),
		syslog_identity_m(syslog_identity)
	{
		init(syslog_identity_m);
	}
	
	void init(const std::string& syslog_identity) {
		if (syslog_identity_m.empty()) {
			std::string error_msg = "can not create logger with empty identity";
			error_msg += " for SYSLOG_LOGGER";
			throw internal_error(error_msg);
		}

		closelog();
		syslog_identity_m = syslog_identity;

		openlog(syslog_identity_m.c_str(), LOG_PID | LOG_NDELAY, LOG_LOCAL1);

		if ((flags_m & PLOG_INTRO) == PLOG_INTRO) {
			syslog(LOG_NOTICE, "%s", first_message_m.c_str());
		}
	}
	
	~syslog_logger_t () {
		closelog();
	}
	
private:
	void internal_log(unsigned int type, const std::string& message) {		
		if ((flags_m & type) != type) {
			return;
		}
		
		syslog(LOG_NOTICE, "%s%s\n", get_message_prefix(type).c_str(), message.c_str());
	}

	std::string syslog_identity_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SMART_LOGGER_HPP_INCLUDED_
