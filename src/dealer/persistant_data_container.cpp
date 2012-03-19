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

#include <cstring>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>

#include <openssl/sha.h>
#include <openssl/evp.h>

#include "json/json.h"

#include "cocaine/dealer/details/error.hpp"
#include "cocaine/dealer/details/persistant_data_container.hpp"

namespace cocaine {
namespace dealer {

persistant_data_container::persistant_data_container() :
	data_in_memory_(false),
	data_(NULL),
	size_(0)
{
}

persistant_data_container::persistant_data_container(const void* data, size_t size) :
	data_in_memory_(false),
	data_(NULL),
	size_(0)
{
	data_in_memory_ = true;

	// early exit
	if (data == NULL || size == 0) {
		data_ = NULL;
		size_ = 0;
		return;
	}

	size_ = size;

	allocate_memory();
	memcpy(data_, data, size_);
}

persistant_data_container::persistant_data_container(const persistant_data_container& dc)
{
	*this = dc;
}

persistant_data_container::~persistant_data_container() {
	unload_data();
}

void
persistant_data_container::unload_data() {
	if (data_) {
		delete [] data_;
		data_ = NULL;
	}

	data_in_memory_ = false;
}

void
persistant_data_container::load_data() {
	assert(data_ == NULL);
	allocate_memory();

	// read into allocated memory
	std::string str = blob_.read(uuid_, EBLOB_COLUMN);
	memcpy(data_, str.data(), size_);

	data_in_memory_ = true;
}

void*
persistant_data_container::data() const {
	return data_;
}

size_t
persistant_data_container::size() const {
	return size_;
}

bool
persistant_data_container::empty() const {
	return (size_ == 0);
}

void
persistant_data_container::allocate_memory() {
	std::string error_msg = "not enough memory to create new data container at ";
	error_msg += std::string(BOOST_CURRENT_FUNCTION);

	try {
		data_ = new unsigned char[size_];
	}
	catch (...) {
		throw error(error_msg);
	}

	if (!data_) {
		throw error(error_msg);
	}
}

void
persistant_data_container::set_eblob(eblob blob, const std::string& uuid) {
	blob_ = blob;
	uuid_ = uuid;
}

void
persistant_data_container::commit_data() {
	if (!data_in_memory_) {
		return;
	}

	blob_.write(uuid_, data_, size_, EBLOB_COLUMN);
	unload_data();
	data_in_memory_ = false;
}

persistant_data_container&
persistant_data_container::operator = (const persistant_data_container& rhs) {
	blob_ = rhs.blob_;
	uuid_ = rhs.uuid_;
}

bool
persistant_data_container::operator == (const persistant_data_container& rhs) const {
	return (uuid_ == rhs.uuid_);
}

bool
persistant_data_container::operator != (const persistant_data_container& rhs) const {
	return !(*this == rhs);
}

bool
persistant_data_container::is_data_loaded() {
	return data_in_memory_;
}

} // namespace dealer
} // namespace cocaine
