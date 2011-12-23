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

#ifndef _LSD_CACHED_MESSAGE_HPP_INCLUDED_
#define _LSD_CACHED_MESSAGE_HPP_INCLUDED_

#include <string>
#include <sys/time.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/details/data_container.hpp"
#include "cocaine/dealer/details/time_value.hpp"

namespace lsd {

class cached_message {
public:
	cached_message();
	explicit cached_message(const cached_message& message);

	cached_message(const message_path& path,
				   const message_policy& policy,
				   const void* data,
				   size_t data_size_);

	virtual ~cached_message();

	const data_container& data() const;
	size_t container_size() const;

	const message_path& path() const;
	const message_policy& policy() const;
	const std::string& uuid() const;

	bool is_sent() const;
	const time_value& sent_timestamp() const;

	void mark_as_sent(bool value);

	std::string json();
	bool is_expired();

	cached_message& operator = (const cached_message& rhs);
	bool operator == (const cached_message& rhs) const;
	bool operator != (const cached_message& rhs) const;

	static const size_t MAX_MESSAGE_DATA_SIZE = 2147483648; // 2gb
	static const size_t UUID_SIZE = 36; // bytes

private:
	void gen_uuid();
	void init();
	
private:
	// message data
	data_container data_;
	message_path path_;
	message_policy policy_;
	std::string uuid_;

	// metadata
	bool is_sent_;
	time_value sent_timestamp_;
	size_t container_size_;
	int timeout_retries_count_;

	// synchronization
	boost::mutex mutex_;
};

} // namespace lsd

#endif // _LSD_CACHED_MESSAGE_HPP_INCLUDED_
