//
// Copyright (C) 2011 Rim Zaidullin <creator@bash.org.ru>
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

#include <cstdio>
#include <cstdarg>
#include <cstring>

#include "cocaine/dealer/details/error.hpp"

namespace lsd {

int const error::MESSAGE_SIZE;

error::error(const std::string& format, ...) :
	std::exception(),
	type_(LSD_UNKNOWN_ERROR)
{
	va_list args;
	va_start(args, format);
	vsnprintf(message_, MESSAGE_SIZE, format.c_str(), args);
	va_end(args);
}

error::error(enum error_type type, const std::string& format, ...) :
	std::exception(),
	type_(type)
{
	va_list args;
	va_start(args, format);
	vsnprintf(message_, MESSAGE_SIZE, format.c_str(), args);
	va_end(args);
}

error::~error() throw () {
}

error::error(const error& other) : std::exception(other) {
	memcpy(message_, other.message_, MESSAGE_SIZE);
}

error&
error::operator = (const error& other) {
	memcpy(message_, other.message_, MESSAGE_SIZE);
	return *this;
}

char const*
error::what() const throw () {
	return message_;
}

} // namespace lsd
