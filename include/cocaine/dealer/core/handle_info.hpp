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

#ifndef _COCAINE_DEALER_HANDLE_INFO_HPP_INCLUDED_
#define _COCAINE_DEALER_HANDLE_INFO_HPP_INCLUDED_

#include <string>
#include <map>

#include <boost/lexical_cast.hpp>

#include "cocaine/dealer/structs.hpp"

namespace cocaine {
namespace dealer {

struct handle_info_t {
public:
	handle_info_t() {};

	handle_info_t(const std::string& name_,
				  const std::string& app_name_,
				  const std::string& service_alias_) :
		name(name_),
		app_name(app_name_),
		service_alias(service_alias_) {};

	handle_info_t(const handle_info_t& rhs) :
		name(rhs.name),
		app_name(rhs.app_name),
		service_alias(rhs.service_alias) {};

	bool operator == (const handle_info_t& rhs) const {
		return (name == rhs.name &&
				app_name == rhs.app_name &&
				service_alias == rhs.service_alias);
	};

	std::string as_string() const {
		return "[" + service_alias + "].[" + app_name + "].[" + name + "]";
	}

	std::string name;
	std::string app_name;
	std::string service_alias;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HANDLE_INFO_HPP_INCLUDED_
