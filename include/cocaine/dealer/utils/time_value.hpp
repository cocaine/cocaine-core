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

#ifndef _COCAINE_DEALER_TIME_VALUE_HPP_INCLUDED_
#define _COCAINE_DEALER_TIME_VALUE_HPP_INCLUDED_

#include <string>

#include <sys/time.h>

#include <msgpack.hpp>

namespace cocaine {
namespace dealer {

class time_value;
std::ostream& operator<<(std::ostream& out, time_value& tval);

class time_value {
public:
	time_value();
	time_value(const time_value& tv);
	time_value(const timeval& tv);
	time_value(double tv);
	virtual ~time_value();

	std::string as_string() const;
	double as_double() const;
	timeval as_timeval() const;

	long days() const;
	long hours() const;
	long minutes() const;
	long seconds() const;
	long milliseconds() const;
	long microseconds() const;

	time_value& operator = (const time_value& rhs);

	bool operator == (const time_value& rhs) const;
	bool operator != (const time_value& rhs) const;
	bool operator > (const time_value& rhs) const;
	bool operator >= (const time_value& rhs) const;
	bool operator < (const time_value& rhs) const;
	bool operator <= (const time_value& rhs) const;

	bool empty() const;
	double distance(const time_value& rhs) const;
	void init_from_current_time();
	void drop_microseconds();
	void reset();

	time_value operator + (double interval);
	time_value operator - (double interval);
	time_value& operator += (double interval);
	time_value& operator -= (double interval);

	static time_value get_current_time();

	MSGPACK_DEFINE(value_.tv_sec, value_.tv_usec);

private:
	timeval value_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_TIME_VALUE_HPP_INCLUDED_
