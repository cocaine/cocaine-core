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

#ifndef _COCAINE_DEALER_ERROR_HPP_INCLUDED_
#define _COCAINE_DEALER_ERROR_HPP_INCLUDED_

#include <exception>
#include <string>

namespace cocaine {
namespace dealer {

enum error_type {
	DEALER_UNKNOWN_ERROR = 1,
	DEALER_MESSAGE_DATA_TOO_BIG_ERROR,
	DEALER_MESSAGE_CACHE_OVER_CAPACITY_ERROR,
	DEALER_OVER_HDD_CAPACITY_ERROR,
	DEALER_UNKNOWN_SERVICE_ERROR
};

class error : public std::exception {
public:
	error(const std::string& format, ...);
	error(enum error_type type, const std::string& format, ...);

	virtual ~error() throw ();

	error(error const& other);
	error& operator = (error const& other);

	static const int MESSAGE_SIZE = 512;

	virtual char const* what() const throw ();

private:
	char message_[MESSAGE_SIZE];
	enum error_type type_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_ERROR_HPP_INCLUDED_
