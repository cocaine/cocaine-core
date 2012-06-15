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

class base_logger {
public:
	base_logger() {
		flags_ = PLOG_ALL;
		is_paused_ = false;
		
		create_first_message();
	}

	base_logger(unsigned int flags) {
		flags_ = flags;
		is_paused_ = false;
		
		if ((flags_ & PLOG_INTRO) == PLOG_INTRO) {
			create_first_message();
		}
	}
	
	std::string get_message_prefix(unsigned int message_type) {
		std::string prefix;

		time_t now = time(NULL);
		if ((flags_ & PLOG_TIME) == PLOG_TIME) {
			struct tm* ptm = localtime(&now);
			char buffer[32];
			strftime (buffer, 32, "%H:%M:%S", ptm);
			prefix += "[";
			prefix += buffer;
			prefix += "]";
		}

		if ((flags_ & PLOG_TYPES) == PLOG_TYPES) {
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
			prefix += "    ";
		}
		
		return prefix;
	}

	virtual unsigned int flags() const { return 0; };
	virtual void set_flags( __attribute__ ((unused)) unsigned int flags) {};
	virtual void set_paused( __attribute__ ((unused)) bool value) {};
	virtual bool is_paused() { return false; };
	virtual void log(__attribute__ ((unused)) const std::string& message) {};
	virtual void log(__attribute__ ((unused)) unsigned int type,  __attribute__ ((unused)) const std::string& message) {};
	virtual void log(__attribute__ ((unused)) const char* message, ...) {};
	virtual void log(__attribute__ ((unused)) unsigned int type,  __attribute__ ((unused)) const char* message, ...) {};

	bool is_paused_;
	unsigned int flags_;
	std::string first_message_;

private:
	void create_first_message() {
		if (flags_ == PLOG_NONE) {
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

		first_message_ = "---------------------- new log started at ";
		first_message_ += std::string(buffer) + " ----------------------\n";	
	}
};

class empty_logger : public base_logger {
public:	
	void log(__attribute__ ((unused)) unsigned int type, __attribute__ ((unused)) const std::string& message) {
	}
};

class stdout_logger : public base_logger {
public:
	stdout_logger() {
		if ((flags_ & PLOG_INTRO) == PLOG_INTRO) {
			std::cout << first_message_ << std::flush;
		}
	}

	stdout_logger(unsigned int flags) : base_logger(flags) {
		if ((flags_ & PLOG_INTRO) == PLOG_INTRO) {
			std::cout << first_message_ << std::flush;
		}
	}
	
	static void log_common(const std::string& message) {
		std::cout << message << "\n" << std::flush;
	}
	
	void log(unsigned int type, const std::string& message) {
		if ((flags_ & type) != type || is_paused_) {
			return;
		}

		std::cout << get_message_prefix(type) << message << "\n" << std::flush;
	}
};

class file_logger : public base_logger {
public:
	file_logger(unsigned int flags) : base_logger(flags) {};
	
	void init(const std::string& file_path) {				
		file_path_ = file_path;

		std::string error_msg = "logger creation failed. unable to open file: ";
		error_msg += file_path_;
		error_msg += " for FILE_LOGGER";

		try {
			file_.open(file_path_.c_str(), std::fstream::out | std::fstream::app);
		}
		catch (...) {
			file_.close();
			throw internal_error(error_msg + " at: " + std::string(BOOST_CURRENT_FUNCTION));
		}

		if (!file_.is_open()) {
			file_.close();
			throw internal_error(error_msg + " at: " + std::string(BOOST_CURRENT_FUNCTION));
		}

		if ((flags_ & PLOG_INTRO) == PLOG_INTRO) {
			file_ << first_message_;
		}
	}
	
	virtual ~file_logger () {
		file_.close();
	}

	void log(unsigned int type, const std::string& message) {		
		if ((flags_ & type) != type || is_paused_) {
			return;
		}
		
		std::string error_msg = "writing to log failed. file: ";
		error_msg += file_path_;
		error_msg += " is closed for FILE_LOGGER";

		if (!file_.is_open()) {
			file_.close();
			throw internal_error(error_msg + "at: " + std::string(BOOST_CURRENT_FUNCTION));
		}

		file_ << get_message_prefix(type) << message << "\n" << std::flush;
	}
	
private:
	std::ofstream file_;
	std::string file_path_;
};

class syslog_logger : public base_logger {
public:
	syslog_logger(unsigned int flags) : base_logger(flags) {};
	
	void init(const std::string& syslog_name) {				
		syslog_name_ = syslog_name;

		openlog(syslog_name_.c_str(), LOG_PID | LOG_NDELAY, LOG_LOCAL1);

		if ((flags_ & PLOG_INTRO) == PLOG_INTRO) {
			syslog(LOG_NOTICE, "%s", first_message_.c_str());
		}
	}
	
	virtual ~syslog_logger () {
		closelog();
	}

	void log(unsigned int type, const std::string& message) {		
		if ((flags_ & type) != type || is_paused_) {
			return;
		}
		
		syslog(LOG_NOTICE, "%s%s\n", get_message_prefix(type).c_str(), message.c_str());
	}
	
private:
	std::string syslog_name_;
};

template<class logging_policy = stdout_logger>
class smart_logger : public logging_policy {
public:
	smart_logger() {};
	smart_logger(unsigned int flags) : logging_policy(flags) {};

	unsigned int flags() const {
		return static_cast<const logging_policy*>(this)->flags_;
	};

	void set_flags(unsigned int flags) {
		static_cast<logging_policy*>(this)->flags_ = flags;
	}
	
	bool is_paused() const {
		return static_cast<const logging_policy*>(this)->is_paused_;
	}

	void set_paused(bool value) {
		static_cast<logging_policy*>(this)->is_paused_ = value;
	}
		
	void log(const std::string& message) {
		static_cast<logging_policy*>(this)->log(PLOG_INFO, message);
	}
	
	void log(unsigned int type, const char* message, ...) {
		bool is_paused = static_cast<logging_policy*>(this)->is_paused_;
		unsigned int flags = static_cast<logging_policy*>(this)->flags_;
		
		if ((flags & type) != type || is_paused) {
			return;
		}

		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message, vl);
		va_end(vl);
	
		static_cast<logging_policy*>(this)->log(type, std::string(buff));
	}

	void log(const char* message, ...) {
		bool is_paused = static_cast<logging_policy*>(this)->is_paused_;
		unsigned int flags = static_cast<logging_policy*>(this)->flags_;
		
		if ((flags & PLOG_INFO) != PLOG_INFO || is_paused) {
			return;
		}

		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message, vl);
		va_end(vl);
	
		static_cast<logging_policy*>(this)->log(PLOG_INFO, std::string(buff));
	}
	
	static void log_common(const std::string& message) {
		logging_policy::log_common(message);
	}
	
	static void log_common(const char* message, ...) {
		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message, vl);
		va_end(vl);
		
		logging_policy::log_common(std::string(buff));
	}
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SMART_LOGGER_HPP_INCLUDED_
