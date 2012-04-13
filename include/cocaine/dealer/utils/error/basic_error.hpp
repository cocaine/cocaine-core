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
