//
// Copyright (C) 2011-2012 Rim Zaidullin <tinybit@yandex.ru>
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

#ifndef _COCAINE_DEALER_EBLOB_STORAGE_HPP_INCLUDED_
#define _COCAINE_DEALER_EBLOB_STORAGE_HPP_INCLUDED_

#include <string>
#include <map>
#include <stdexcept>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/lexical_cast.hpp>

#include <eblob/eblob.hpp>

#include "cocaine/dealer/utils/smart_logger.hpp"
#include "cocaine/dealer/storage/eblob.hpp"

namespace cocaine {
namespace dealer {

class eblob_storage : private boost::noncopyable {
public:
	eblob_storage(std::string path,
				  uint64_t blob_size = eblob::DEFAULT_BLOB_SIZE,
				  int sync_interval = eblob::DEFAULT_SYNC_INTERVAL,
				  int defrag_timeout = eblob::DEFAULT_DEFRAG_TIMEOUT) :
		path_(path),
		blob_size_(blob_size),
		sync_interval_(sync_interval),
		defrag_timeout_(defrag_timeout)
	{
		// add slash to path if missing
		if (path_.at(path_.length() - 1) != '/') {
			path_ += "/";
		}
	}

	virtual ~eblob_storage() {};

	void open_eblob(const std::string& nm) {
		std::map<std::string, eblob>::const_iterator it = eblobs_.find(nm);

		// eblob is already open
		if (it != eblobs_.end()) {
			return;
		}

		// create eblob
		eblob eb(path_ + nm, blob_size_, sync_interval_, defrag_timeout_);
		eblobs_.insert(std::make_pair(nm, eb));
	}

	eblob operator[](const std::string& nm) {
		return get_eblob(nm);
	}

	eblob get_eblob(const std::string& nm) {
		std::map<std::string, eblob>::const_iterator it = eblobs_.find(nm);

		// no such eblob was opened
		if (it == eblobs_.end()) {
			std::string error_msg = "no eblob storage object with path: " + path_ + nm;
			error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw std::runtime_error(error_msg);
		}

		return it->second;
	}

	void close_eblob(const std::string& nm) {
		std::map<std::string, eblob>::iterator it = eblobs_.find(nm);

		// eblob is already open
		if (it == eblobs_.end()) {
			return;
		}

		eblobs_.erase(it);
	}

private:
	std::map<std::string, eblob> eblobs_;

	std::string path_;
	uint64_t blob_size_;
	int sync_interval_;
	int defrag_timeout_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_EBLOB_STORAGE_HPP_INCLUDED_
