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

#ifndef _COCAINE_DEALER_CACHED_RESPONSE_HPP_INCLUDED_
#define _COCAINE_DEALER_CACHED_RESPONSE_HPP_INCLUDED_

#include <string>
#include <sys/time.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/utils/data_container.hpp"

namespace cocaine {
namespace dealer {

class cached_response_t {
public:
	cached_response_t();
	explicit cached_response_t(const cached_response_t& response);

	cached_response_t(const std::string& uuid,
					  const std::string& route,
					  const message_path& path,
					  const void* data,
					  size_t data_size);

	cached_response_t(const std::string& uuid,
					  const std::string& route,
					  const message_path& path,
					  int code,
					  const std::string& error_message);

	virtual ~cached_response_t();

	const data_container& data() const;
	size_t container_size() const;

	const message_path& path() const;
	const std::string& uuid() const;
	const std::string& route() const;

	int code() const;
	std::string error_message() const;

	void set_code(int code);
	void set_error_message(const std::string& message);

	const timeval& received_timestamp() const;
	void set_received_timestamp(const timeval& val);

	cached_response_t& operator = (const cached_response_t& rhs);
	bool operator == (const cached_response_t& rhs) const;
	bool operator != (const cached_response_t& rhs) const;

	static const size_t MAX_RESPONSE_DATA_SIZE = 2147483648; // 2gb
	static const size_t UUID_SIZE = 36; // bytes
	
private:
	std::string uuid_;
	std::string route_;
	message_path path_;
	data_container data_;

	timeval received_timestamp_;
	size_t container_size_;

	int code_;
	std::string error_message_;

	// synchronization
	boost::mutex mutex_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CACHED_RESPONSE_HPP_INCLUDED_
