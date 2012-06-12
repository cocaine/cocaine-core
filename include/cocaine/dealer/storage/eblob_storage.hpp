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
#include "cocaine/dealer/utils/error.hpp"
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
			throw internal_error(error_msg);
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
