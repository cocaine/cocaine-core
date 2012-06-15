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

class eblob_storage_t : private boost::noncopyable, public dealer_object_t {
public:
	eblob_storage_t(std::string path,
				  const boost::shared_ptr<context_t>& ctx,
				  bool logging_enabled = true,
				  uint64_t blob_size = eblob_t::DEFAULT_BLOB_SIZE,
				  int sync_interval = eblob_t::DEFAULT_SYNC_INTERVAL,
				  int defrag_timeout = eblob_t::DEFAULT_DEFRAG_TIMEOUT) :
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

	virtual ~eblob_storage_t() {};

	void open_eblob(const std::string& nm) {
		std::map<std::string, boost::shared_ptr<eblob_t> >::const_iterator it = m_eblobs.find(nm);

		// eblob_t is already open
		if (it != m_eblobs.end()) {
			return;
		}

		// create eblob_t
		boost::shared_ptr<eblob_t> eb(new eblob_t(m_path + nm,
												  context(),
												  true,
												  m_blob_size,
												  m_sync_interval,
												  m_defrag_timeout));

		m_eblobs.insert(std::make_pair(nm, eb));
	}

	boost::shared_ptr<eblob_t> operator[](const std::string& nm) {
		return get_eblob(nm);
	}

	boost::shared_ptr<eblob_t> get_eblob(const std::string& nm) {
		std::map<std::string, boost::shared_ptr<eblob_t> >::const_iterator it = m_eblobs.find(nm);

		// no such eblob_t was opened
		if (it == m_eblobs.end()) {
			std::string error_msg = "no eblob_t storage object with path: " + m_path + nm;
			error_msg += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw internal_error(error_msg);
		}

		return it->second;
	}

	void close_eblob(const std::string& nm) {
		std::map<std::string, boost::shared_ptr<eblob_t> >::iterator it = m_eblobs.find(nm);

		// eblob_t is already open
		if (it == m_eblobs.end()) {
			return;
		}

		m_eblobs.erase(it);
	}

private:
	std::map<std::string, boost::shared_ptr<eblob_t> > m_eblobs;

	std::string	m_path;
	uint64_t	m_blob_size;
	int 		m_sync_interval;
	int			m_defrag_timeout;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_EBLOB_STORAGE_HPP_INCLUDED_
