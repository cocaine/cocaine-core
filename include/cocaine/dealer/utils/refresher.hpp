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

#ifndef _COCAINE_DEALER_REFRESHER_HPP_INCLUDED_
#define _COCAINE_DEALER_REFRESHER_HPP_INCLUDED_

#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

namespace cocaine {
namespace dealer {

class refresher : private boost::noncopyable {

public:
	refresher(boost::function<void()> f, boost::uint32_t timeout); // timeout in millisecs
	virtual ~refresher();

private:
	void refreshing_thread();

private:
	boost::function<void()> f_;
	boost::uint32_t timeout_;
	volatile bool stopping_;
	boost::mutex m_mutex;
	boost::thread refreshing_thread_;

};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_REFRESHER_HPP_INCLUDED_
