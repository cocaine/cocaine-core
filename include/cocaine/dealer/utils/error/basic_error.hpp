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

#ifndef _COCAINE_BASIC_ERROR_HPP_INCLUDED_
#define _COCAINE_BASIC_ERROR_HPP_INCLUDED_

#include <exception>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace cocaine {
namespace dealer {

class basic_error : public std::exception {
public:
	basic_error() : std::exception() {
	}

	basic_error(const std::string& format, ...) : std::exception() {
		va_list args;
		va_start(args, format);
		vsnprintf(message_, ERROR_MESSAGE_SIZE, format.c_str(), args);
		va_end(args);
	}

	virtual ~basic_error() throw () {
	}

	basic_error(basic_error const& other) : std::exception(other) {
		memcpy(message_, other.message_, ERROR_MESSAGE_SIZE);
	}

	basic_error& operator = (basic_error const& other) {
		memcpy(message_, other.message_, ERROR_MESSAGE_SIZE);
		return *this;
	}

	static const int ERROR_MESSAGE_SIZE = 512;

	virtual char const* what() const throw () {
		return message_;
	}

protected:
	char message_[ERROR_MESSAGE_SIZE];
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_BASIC_ERROR_HPP_INCLUDED_
