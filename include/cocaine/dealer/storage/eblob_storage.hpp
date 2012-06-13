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
#include "cocaine/dealer/core/dealer_object.hpp"

namespace cocaine {
namespace dealer {

class eblob_storage : private boost::noncopyable, public dealer_object_t {
public:
	eblob_storage(std::string path,
				  const boost::shared_ptr<context_t>& ctx,
				  bool logging_enabled = true,
				  uint64_t blob_size = eblob::DEFAULT_BLOB_SIZE,
				  int sync_interval = eblob::DEFAULT_SYNC_INTERVAL,
				  int defrag_timeout = eblob::DEFAULT_DEFRAG_TIMEOUT) :
		dealer_object_t(ctx, logging_enabled),
		m_path(path),
		m_blob_size(blob_size),
		m_sync_interval(sync_interval),
		m_defrag_timeout(defrag_timeout)
	{
		// add slash to path if missing
		if (m_path.at(m_path.length() - 1) != '/') {
			m_path += "/";
		}
	}

	virtual ~eblob_storage() {};

	void open_eblob(const std::string& nm) {
		std::map<std::string, boost::shared_ptr<eblob> >::const_iterator it = eblobs_.find(nm);

		// eblob is already open
		if (it != eblobs_.end()) {
			return;
		}

		// create eblob
		boost::shared_ptr<eblob> eb(new eblob(m_path + nm,
											  context(),
											  true,
											  m_blob_size,
											  m_sync_interval,
											  m_defrag_timeout));

		eblobs_.insert(std::make_pair(nm, eb));
	}

	boost::shared_ptr<eblob> operator[](const std::string& nm) {
		return get_eblob(nm);
	}

	boost::shared_ptr<eblob> get_eblob(const std::string& nm) {
		std::map<std::string, boost::shared_ptr<eblob> >::const_iterator it = eblobs_.find(nm);

		// no such eblob was opened
		if (it == eblobs_.end()) {
			std::string error_msg = "no eblob storage object with path: " + m_path + nm;
			error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw internal_error(error_msg);
		}

		return it->second;
	}

	void close_eblob(const std::string& nm) {
		std::map<std::string, boost::shared_ptr<eblob> >::iterator it = eblobs_.find(nm);

		// eblob is already open
		if (it == eblobs_.end()) {
			return;
		}

		eblobs_.erase(it);
	}

private:
	std::map<std::string, boost::shared_ptr<eblob> > eblobs_;

	std::string m_path;
	uint64_t m_blob_size;
	int m_sync_interval;
	int m_defrag_timeout;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_EBLOB_STORAGE_HPP_INCLUDED_
