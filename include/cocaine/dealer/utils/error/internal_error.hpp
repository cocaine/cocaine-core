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

#ifndef _COCAINE_INTERNAL_ERROR_HPP_INCLUDED_
#define _COCAINE_INTERNAL_ERROR_HPP_INCLUDED_

#include <cocaine/dealer/utils/error/basic_error.hpp>
#include <cocaine/dealer/types.hpp>

namespace cocaine {
namespace dealer {

class internal_error : public basic_error {
public:
	internal_error(const std::string& format, ...) :
		basic_error()
	{
		va_list args;
		va_start(args, format);
		vsnprintf(message_, ERROR_MESSAGE_SIZE, format.c_str(), args);
		va_end(args);
	}

	virtual ~internal_error() throw () {
	}

	internal_error(internal_error const& other) : basic_error() {
		memcpy(message_, other.message_, ERROR_MESSAGE_SIZE);
	}
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_INTERNAL_ERROR_HPP_INCLUDED_
