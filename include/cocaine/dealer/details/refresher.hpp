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

#ifndef _LSD_REFRESHER_HPP_INCLUDED_
#define _LSD_REFRESHER_HPP_INCLUDED_

#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

namespace lsd {

class refresher : private boost::noncopyable {

public:
	refresher(boost::function<void()> f, boost::uint32_t timeout_seconds); // timeout in secs
	virtual ~refresher();

private:
	void refreshing_thread();

private:
	boost::function<void()> f_;
	boost::uint32_t timeout_;
	volatile bool stopping_;
	boost::condition condition_;
	boost::mutex mutex_;
	boost::thread refreshing_thread_;

};

} // namespace lsd

#endif // _LSD_REFRESHER_HPP_INCLUDED_
