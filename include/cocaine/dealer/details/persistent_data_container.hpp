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

#ifndef _COCAINE_DEALER_PERSISTENT_DATA_CONTAINER_HPP_INCLUDED_
#define _COCAINE_DEALER_PERSISTENT_DATA_CONTAINER_HPP_INCLUDED_

#include <string>
#include <cstring>
#include <sys/time.h>

#include <boost/shared_ptr.hpp>
#include <boost/detail/atomic_count.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/details/data_container.hpp"
#include "cocaine/dealer/details/eblob.hpp"

namespace cocaine {
namespace dealer {

class persistent_data_container {
public:
	persistent_data_container();
	persistent_data_container(const void* data, size_t size);
	persistent_data_container(const persistent_data_container& dc);
	virtual ~persistent_data_container();

	void init_from_message_cache(eblob blob, const std::string& uuid, int64_t data_size);

	persistent_data_container& operator = (const persistent_data_container& rhs);
	bool operator == (const persistent_data_container& rhs) const;
	bool operator != (const persistent_data_container& rhs) const;

	void set_eblob(eblob blob, const std::string& uuid);
	void commit_data();

	void set_data(const void* data, size_t size);

	void* data() const;
	size_t size() const;
	bool empty() const;

	bool is_data_loaded();
	void load_data();
	void unload_data();

	void remove_from_persistent_cache();

	static const size_t EBLOB_COLUMN = 1;

protected:
	void allocate_memory();

protected:
	// persistant storage
	eblob blob_;
	bool data_in_memory_;

	// data
	unsigned char* data_;
	size_t size_;

	// key to store data in eblob
	std::string uuid_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_PERSISTENT_DATA_CONTAINER_HPP_INCLUDED_
