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

#ifndef _COCAINE_DEALER_MESSAGE_IFACE_HPP_INCLUDED_
#define _COCAINE_DEALER_MESSAGE_IFACE_HPP_INCLUDED_

#include <string>

#include "cocaine/dealer/details/time_value.hpp"
#include "cocaine/dealer/structs.hpp"

namespace cocaine {
namespace dealer {

class message_iface {
public:
	virtual ~message_iface() {};

	virtual void* data() = 0;
	virtual size_t size() const = 0;

	virtual bool is_data_loaded() = 0;
	virtual void load_data() = 0;
	virtual void unload_data() = 0;

	virtual size_t container_size() const = 0;

	virtual const message_path& path() const = 0;
	virtual const message_policy& policy() const = 0;
	virtual const std::string& uuid() const = 0;

	virtual bool is_sent() const = 0;
	virtual const time_value& sent_timestamp() const = 0;

	virtual void mark_as_sent(bool value) = 0;

	virtual std::string json() = 0;
	virtual bool is_expired() = 0;

	virtual message_iface& operator = (const message_iface& rhs) = 0;
	virtual bool operator == (const message_iface& rhs) const = 0;
	virtual bool operator != (const message_iface& rhs) const = 0;

	static const size_t MAX_MESSAGE_DATA_SIZE = 2147483648; // 2 gb
	static const size_t UUID_SIZE = 36; // bytes
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_MESSAGE_IFACE_HPP_INCLUDED_
