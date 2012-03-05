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

#ifndef _COCAINE_HELPERS_DATA_CONTAINER_HPP_INCLUDED_
#define _COCAINE_HELPERS_DATA_CONTAINER_HPP_INCLUDED_

#include <boost/shared_ptr.hpp>
#include <boost/detail/atomic_count.hpp>
#include <boost/thread/mutex.hpp>

namespace cocaine { 
namespace helpers {

class data_container_t {

public:
	data_container_t();
	data_container_t(const void* data, size_t size);
	data_container_t(const data_container_t& dc);
	virtual ~data_container_t();
	
	data_container_t& operator = (const data_container_t& rhs);
	bool operator == (const data_container_t& rhs) const;
	bool operator != (const data_container_t& rhs) const;

	void* data() const;
	size_t size() const;
	bool empty() const;
	void clear();

private:
	// sha1 size in bytes
	static const size_t SHA1_SIZE = 20;

	// sha1-encoded data chunk size - 512 kb
	static const size_t SHA1_CHUNK_SIZE = 512 * 1024;

	// max amount of data that does not need sha1 signature 1 mb
	static const size_t SMALL_DATA_SIZE = 1024 * 1024;

	typedef boost::detail::atomic_count reference_counter;
	
	void init();
	void init_with_data(unsigned char* data, size_t size);
	void swap(data_container_t& other);
	void sign_data(unsigned char* data, size_t& size, unsigned char signature[SHA1_SIZE]);

private:
	// data
	unsigned char* data_;
	size_t size_;

	// data sha1 signature
	bool signed_;
	unsigned char signature_[SHA1_SIZE];

	// data reference counter
	boost::shared_ptr<reference_counter> ref_counter_;

	// synchronization
	boost::mutex mutex_;
};

} // namespace helpers
} // namespace cocaine

#endif // _COCAINE_HELPERS_DATA_CONTAINER_HPP_INCLUDED_
