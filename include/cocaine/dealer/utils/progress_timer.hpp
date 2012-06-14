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

#ifndef _COCAINE_DEALER_PROGRESS_TIMER_HPP_INCLUDED_
#define _COCAINE_DEALER_PROGRESS_TIMER_HPP_INCLUDED_

#include "cocaine/dealer/utils/time_value.hpp"

namespace cocaine {
namespace dealer {

class progress_timer {

public:
	progress_timer();
	virtual ~progress_timer();

	void reset();
	time_value elapsed();

private:
	time_value begin_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_PROGRESS_TIMER_HPP_INCLUDED_
