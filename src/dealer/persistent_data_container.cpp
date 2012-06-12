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

#include <cstring>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>

#include <openssl/sha.h>
#include <openssl/evp.h>

#include "json/json.h"

#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/persistent_data_container.hpp"

namespace cocaine {
namespace dealer {

persistent_data_container::persistent_data_container() :
	data_in_memory_(false),
	data_(NULL),
	size_(0)
{
}

persistent_data_container::persistent_data_container(const void* data, size_t size) :
	data_in_memory_(false),
	data_(NULL),
	size_(0)
{
	set_data(data, size);
}

persistent_data_container::persistent_data_container(const persistent_data_container& dc)
{
	*this = dc;
}

persistent_data_container::~persistent_data_container() {
	unload_data();
}

void
persistent_data_container::set_data(const void* data, size_t size) {
	if (data_in_memory_) {
		return;
	}

	data_in_memory_ = true;

	// early exit
	if (data == NULL || size == 0) {
		data_ = NULL;
		size_ = 0;
		return;
	}

	size_ = size;

	if (size_ > 0) {
		allocate_memory();
		memcpy(data_, data, size_);
	}
}

void
persistent_data_container::unload_data() {
	if (data_) {
		delete [] data_;
		data_ = NULL;
	}

	data_in_memory_ = false;
}

void
persistent_data_container::load_data() {
	if (data_in_memory_) {
		return;
	}

	assert(data_ == NULL);

	// read into allocated memory
	std::string str = blob_.read(uuid_, EBLOB_COLUMN);
	size_ = str.size();
	allocate_memory();
	memcpy(data_, str.data(), size_);

	data_in_memory_ = true;
}

void*
persistent_data_container::data() const {
	return data_;
}

size_t
persistent_data_container::size() const {
	return size_;
}

bool
persistent_data_container::empty() const {
	return (size_ == 0);
}

void
persistent_data_container::allocate_memory() {
	std::string error_msg = "not enough memory to create new data container at ";
	error_msg += std::string(BOOST_CURRENT_FUNCTION);

	if (size_ == 0) {
		return;
	}

	try {
		data_ = new unsigned char[size_];
	}
	catch (...) {
		throw internal_error(error_msg);
	}

	if (!data_) {
		throw internal_error(error_msg);
	}
}

void
persistent_data_container::set_eblob(eblob blob, const std::string& uuid) {
	blob_ = blob;
	uuid_ = uuid;
}

void
persistent_data_container::init_from_message_cache(eblob blob,
												   const std::string& uuid,
												   int64_t data_size)
{
	blob_ = blob;
	uuid_ = uuid;
	size_ = data_size;
}

void
persistent_data_container::commit_data() {
	if (!data_in_memory_) {
		return;
	}

	blob_.write(uuid_, data_, size_, EBLOB_COLUMN);
	//unload_data();
	//data_in_memory_ = false;
}

persistent_data_container&
persistent_data_container::operator = (const persistent_data_container& rhs) {
	if (this != &rhs) {
		blob_ = rhs.blob_;
		uuid_ = rhs.uuid_;
	}

	return *this;
}

bool
persistent_data_container::operator == (const persistent_data_container& rhs) const {
	return (uuid_ == rhs.uuid_);
}

bool
persistent_data_container::operator != (const persistent_data_container& rhs) const {
	return !(*this == rhs);
}

bool
persistent_data_container::is_data_loaded() {
	return data_in_memory_;
}

void
persistent_data_container::remove_from_persistent_cache() {
	blob_.remove_all(uuid_);
}

} // namespace dealer
} // namespace cocaine
