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

#ifndef _COCAINE_DEALER_EBLOB_HPP_INCLUDED_
#define _COCAINE_DEALER_EBLOB_HPP_INCLUDED_

#include <string>
#include <map>
#include <stdexcept>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>
#include <boost/function.hpp>

#include <eblob/eblob.hpp>

#include "cocaine/dealer/core/dealer_object.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"
#include "cocaine/dealer/utils/error.hpp"

namespace cocaine {
namespace dealer {

class eblob : public dealer_object_t {
public:
	typedef boost::function<void(void*, uint64_t, int)> iteration_callback_t;

	eblob();

	eblob(const std::string& path,
		  const boost::shared_ptr<context_t>& ctx,
		  bool logging_enabled = true,
		  uint64_t blob_size = DEFAULT_BLOB_SIZE,
		  int sync_interval = DEFAULT_SYNC_INTERVAL,
		  int defrag_timeout = DEFAULT_DEFRAG_TIMEOUT);

	virtual	~eblob();

	void write(const std::string& key, const std::string& value, int column = EBLOB_TYPE_DATA);
	void write(const std::string& key, void* data, size_t size, int column = EBLOB_TYPE_DATA);
	std::string read(const std::string& key, int column = EBLOB_TYPE_DATA);

	void remove_all(const std::string &key);
	void remove(const std::string& key, int column = EBLOB_TYPE_DATA);

	unsigned long long items_count();

	void iterate(iteration_callback_t iteration_callback, int start_column = 0, int end_column = 99999);

public:
	static const uint64_t DEFAULT_BLOB_SIZE = 2147483648; // 2 gb
	static const int DEFAULT_SYNC_INTERVAL = 2; // secs
	static const int DEFAULT_DEFRAG_TIMEOUT = -1; // secs

private:
	void create_eblob(const std::string& path,
		  			  uint64_t blob_size,
		  			  int sync_interval,
		  			  int defrag_timeout);

	static int iteration_callback(eblob_disk_control* dc,
								  eblob_ram_control* rc,
								  void* data,
								  void* priv,
								  void* thread_priv);

	void iteration_callback_instance(void* data, uint64_t size, int column);

private:
	std::string m_path;
	boost::shared_ptr<ioremap::eblob::eblob> m_storage;
	boost::shared_ptr<ioremap::eblob::eblob_logger> eblob_logger_m;
	iteration_callback_t iteration_callback_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_EBLOB_HPP_INCLUDED_
