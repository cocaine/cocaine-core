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

#ifndef _COCAINE_DEALER_PERSISTENT_DATA_CONTAINER_HPP_INCLUDED_
#define _COCAINE_DEALER_PERSISTENT_DATA_CONTAINER_HPP_INCLUDED_

#include <string>
#include <cstring>
#include <sys/time.h>

#include <boost/shared_ptr.hpp>
#include <boost/detail/atomic_count.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/utils/data_container.hpp"
#include "cocaine/dealer/storage/eblob.hpp"

namespace cocaine {
namespace dealer {

class persistent_data_container {
public:
	persistent_data_container();
	persistent_data_container(const void* data, size_t size);
	persistent_data_container(const persistent_data_container& dc);
	virtual ~persistent_data_container();

	void init_from_message_cache(boost::shared_ptr<eblob> blob, const std::string& uuid, int64_t data_size);

	persistent_data_container& operator = (const persistent_data_container& rhs);
	bool operator == (const persistent_data_container& rhs) const;
	bool operator != (const persistent_data_container& rhs) const;

	void set_eblob(boost::shared_ptr<eblob> blob, const std::string& uuid);
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
	boost::shared_ptr<eblob> blob_;
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
