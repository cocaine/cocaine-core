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

#ifndef _COCAINE_DEALER_DATA_CONTAINER_HPP_INCLUDED_
#define _COCAINE_DEALER_DATA_CONTAINER_HPP_INCLUDED_

#include <string>
#include <cstring>
#include <sys/time.h>

#include <boost/shared_ptr.hpp>
#include <boost/detail/atomic_count.hpp>

#include "cocaine/dealer/structs.hpp"

namespace cocaine {
namespace dealer {

class data_container {

public:
	data_container();
	data_container(const void* data, size_t size);
	data_container(const data_container& dc);
	virtual ~data_container();
	
	data_container& operator = (const data_container& rhs);
	bool operator == (const data_container& rhs) const;
	bool operator != (const data_container& rhs) const;

	void set_data(const void* data, size_t size);

	void* data() const;
	size_t size() const;
	bool empty() const;
	void clear();

	bool is_data_loaded();
	void load_data();
	void unload_data();

	void remove_from_persistent_cache();

protected:
	// sha1 size in bytes
	static const size_t SHA1_SIZE = 20;

	// sha1-encoded data chunk size - 512 kb
	static const size_t SHA1_CHUNK_SIZE = 512 * 1024;

	// max amount of data that does not need sha1 signature 1 mb
	static const size_t SMALL_DATA_SIZE = 1024 * 1024;

	typedef boost::detail::atomic_count reference_counter;
	
	void init();
	void release();

	void sign_data(unsigned char* data, size_t& size, unsigned char signature[SHA1_SIZE]);

protected:
	// data
	unsigned char* data_;
	size_t size_;

	// data sha1 signature
	bool signed_;
	unsigned char signature_[SHA1_SIZE];

	// data reference counter
	boost::shared_ptr<reference_counter> ref_counter_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_DATA_CONTAINER_HPP_INCLUDED_
