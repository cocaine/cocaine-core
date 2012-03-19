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

#ifndef _COCAINE_DEALER_EBLOB_HPP_INCLUDED_
#define _COCAINE_DEALER_EBLOB_HPP_INCLUDED_

#include <string>
#include <map>
#include <stdexcept>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <eblob/eblob.hpp>

#include "cocaine/dealer/details/smart_logger.hpp"

namespace cocaine {
namespace dealer {

class eblob {
public:
	eblob() {}

	eblob(boost::shared_ptr<base_logger> logger,
		  const std::string& path,
		  uint64_t blob_size = DEFAULT_BLOB_SIZE,
		  int sync_interval = DEFAULT_SYNC_INTERVAL,
		  int defrag_timeout = DEFAULT_DEFRAG_TIMEOUT)
	{
		create_eblob(logger, path, blob_size, sync_interval, defrag_timeout);
	}

	eblob(const std::string& path,
		  uint64_t blob_size = DEFAULT_BLOB_SIZE,
		  int sync_interval = DEFAULT_SYNC_INTERVAL,
		  int defrag_timeout = DEFAULT_DEFRAG_TIMEOUT)
	{
		boost::shared_ptr<base_logger> logger;
		create_eblob(logger, path, blob_size, sync_interval, defrag_timeout);
	}

	virtual	~eblob() {
		std::string msg = "eblob at path: ";
		msg += path_ + " destroyed.";
		logger_->log(msg);
	}

	void write(const std::string& key, const std::string& value, int column = zbr::EBLOB_TYPE_DATA) {
		if (!storage_.get()) {
			std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
			throw std::runtime_error(error_msg);
		}

		if (column < 0) {
			std::string error_msg = "bad column index at " + std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
			throw std::runtime_error(error_msg);
		}

		remove(key, column);
		storage_->write_hashed(key, value, 0, 0, column);
	}

	void write(const std::string& key, void* data, size_t size, int column = zbr::EBLOB_TYPE_DATA) {
		if (!storage_.get()) {
			std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
			throw std::runtime_error(error_msg);
		}

		if (column < 0) {
			std::string error_msg = "bad column index at " + std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
			throw std::runtime_error(error_msg);
		}

		std::string value((char*)data, 0, size);
		remove(key, column);
		storage_->write_hashed(key, value, 0, 0, column);
	}

	std::string read(const std::string& key, int column = zbr::EBLOB_TYPE_DATA) {
		if (!storage_.get()) {
			std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
			throw std::runtime_error(error_msg);
		}

		return storage_->read_hashed(key, 0, 0, column);
	}

	void remove_all(const std::string &key) {
		if (!storage_.get()) {
			std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " key: " + key;
			throw std::runtime_error(error_msg);
		}

		zbr::eblob_key ekey;
		storage_->key(key, ekey);
		storage_->remove_all(ekey);
	}

	void remove(const std::string& key, int column = zbr::EBLOB_TYPE_DATA) {
		if (!storage_.get()) {
			std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
			error_msg += " key: " + key + " column: " + boost::lexical_cast<std::string>(column);
			throw std::runtime_error(error_msg);
		}

		storage_->remove_hashed(key, column);
	}

	unsigned long long items_count() {
		if (!storage_.get()) {
			std::string error_msg = "empty eblob storage object at " + std::string(BOOST_CURRENT_FUNCTION);
			throw std::runtime_error(error_msg);
		}

		return storage_->elements();	
	}

	void set_logger(boost::shared_ptr<base_logger> logger) {
		logger_ = logger;
	}

	static const uint64_t DEFAULT_BLOB_SIZE = 2147483648; // 2 gb
	static const int DEFAULT_SYNC_INTERVAL = 2; // secs
	static const int DEFAULT_DEFRAG_TIMEOUT = -1; // secs

private:
	void create_eblob(boost::shared_ptr<base_logger> logger,
		  			  const std::string& path,
		  			  uint64_t blob_size,
		  			  int sync_interval,
		  			  int defrag_timeout)
	{
		path_ = path;

		// create default logger
		logger_ = logger;

		if (!logger_) {
			logger_.reset(new base_logger);
		}

		// create eblob logger
		eblob_logger_.reset(new zbr::eblob_logger(NULL, 0));

		// create config
        zbr::eblob_config cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.file = const_cast<char*>(path_.c_str());
        cfg.log = eblob_logger_->log();
        cfg.sync = sync_interval;
        cfg.blob_size = blob_size;
        cfg.defrag_timeout = defrag_timeout;
        cfg.iterate_threads = 1;

        // create eblob
        storage_.reset(new zbr::eblob(&cfg));
        
        std::string msg = "eblob at path: ";
		msg += path_ + " created.";
		logger_->log(msg);
	}

private:
	std::string path_;
	boost::shared_ptr<zbr::eblob> storage_;
	boost::shared_ptr<zbr::eblob_logger> eblob_logger_;
	boost::shared_ptr<base_logger> logger_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_EBLOB_HPP_INCLUDED_
