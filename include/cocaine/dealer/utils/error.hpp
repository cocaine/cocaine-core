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

#ifndef _COCAINE_DEALER_ERROR_HPP_INCLUDED_
#define _COCAINE_DEALER_ERROR_HPP_INCLUDED_

#include <exception>
#include <string>

namespace cocaine {
namespace dealer {

enum error_type {
	DEALER_UNKNOWN_ERROR = 0,
};

class error : public std::exception {
public:
	error(const std::string& format, ...);
	//error(enum error_type type, const std::string& format, ...);
	error(int type, const std::string& format, ...);

	virtual ~error() throw ();

	error(error const& other);
	error& operator = (error const& other);

	static const int ERROR_MESSAGE_SIZE = 512;

	virtual int type() const throw () {
		return type_;
	}

	virtual char const* what() const throw ();

private:
	char message_[ERROR_MESSAGE_SIZE];
	int type_;
	//enum error_type type_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_ERROR_HPP_INCLUDED_
