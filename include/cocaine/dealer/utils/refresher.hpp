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
	// callback
	boost::function<void()> f_;
	
	// ivars
	boost::uint32_t timeout_;
	volatile bool stopping_;

	// threading
	boost::mutex mutex_;
	boost::condition_variable cond_var_;
	boost::thread refreshing_thread_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_REFRESHER_HPP_INCLUDED_
