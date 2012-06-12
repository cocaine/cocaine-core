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

#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/utils/error.hpp"

namespace cocaine {
namespace dealer {

context_t::context_t(const std::string& config_path) {
	// load configuration from file
	if (config_path.empty()) {
		throw internal_error("config file path is empty string at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	config_.reset(new configuration(config_path));

	// create logger
	switch (config_->logger_type()) {
		case STDOUT_LOGGER:
			logger_.reset(new smart_logger<stdout_logger>(config_->logger_flags()));
			break;
			
		case FILE_LOGGER:
			logger_.reset(new smart_logger<file_logger>(config_->logger_flags()));
			((smart_logger<file_logger>*)logger_.get())->init(config_->logger_file_path());
			break;
			
		case SYSLOG_LOGGER:
			logger_.reset(new smart_logger<syslog_logger>(config_->logger_flags()));
			((smart_logger<syslog_logger>*)logger_.get())->init(config_->logger_syslog_identity());
			break;
			
		default:
			logger_.reset(new smart_logger<empty_logger>);
			break;
	}
	
	logger()->log("loaded config: %s", config()->config_path().c_str());
	//logger()->log(config()->as_string());
	
	// create zmq context
	zmq_context_.reset(new zmq::context_t(1));

	// create statistics collector
	stats_.reset(new statistics_collector(config_, zmq_context_, logger()));

	// create eblob storage
	if (config()->message_cache_type() == PERSISTENT) {
		logger()->log("loading cache from eblobs...");
		std::string st_path = config()->eblob_path();
		int64_t st_blob_size = config()->eblob_blob_size();
		int st_sync = config()->eblob_sync_interval();
		
		// create storage
		eblob_storage* storage_ptr = new eblob_storage(st_path, st_blob_size, st_sync);
		storage_.reset(storage_ptr);

		// create eblob for each service
		const configuration::services_list_t& services_info_list = config()->services_list();
		configuration::services_list_t::const_iterator it = services_info_list.begin();
		for (; it != services_info_list.end(); ++it) {
			storage_->open_eblob(it->second.name_);
			storage_->get_eblob(it->second.name_).set_logger(logger());
		}
	}
}

context_t::~context_t() {
	stats_.reset();
	zmq_context_.reset();
}

boost::shared_ptr<configuration>
context_t::config() {
	return config_;
}

boost::shared_ptr<base_logger>
context_t::logger() {
	return logger_;
}

boost::shared_ptr<zmq::context_t>
context_t::zmq_context() {
	return zmq_context_;
}

boost::shared_ptr<statistics_collector>
context_t::stats() {
	return stats_;
}

boost::shared_ptr<eblob_storage>
context_t::storage() {
	return storage_;
}

} // namespace dealer
} // namespace cocaine
