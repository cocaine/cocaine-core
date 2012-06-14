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

#ifndef _COCAINE_DEALER_STATISTICS_COLLECTOR_HPP_INCLUDED_
#define _COCAINE_DEALER_STATISTICS_COLLECTOR_HPP_INCLUDED_

#include <string>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/thread/thread.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/core/configuration.hpp"

namespace cocaine {
namespace dealer {

enum e_statictics_req_error {
	SRE_BAD_JSON_ERROR = 1,
	SRE_NO_VERSION_ERROR,
	SRE_UNSUPPORTED_VERSION_ERROR,
	SRE_NO_ACTION_ERROR,
	SRE_UNSUPPORTED_ACTION_ERROR
};

class statistics_collector : private boost::noncopyable {
public:
	// status of all services
	typedef std::map<std::string, service_stats> services_stats_t;

	// status of all handles
	typedef std::map<std::pair<std::string, std::string>, handle_stats> handle_stats_t;


public:
	statistics_collector(const boost::shared_ptr<configuration>& config,
						 const boost::shared_ptr<zmq::context_t>& context);

	statistics_collector(const boost::shared_ptr<configuration>& config,
						 const boost::shared_ptr<zmq::context_t>& context,
						 const boost::shared_ptr<base_logger>& logger);

	virtual ~statistics_collector();
	
	std::string get_error_json(enum e_statictics_req_error err) const;

	void set_logger(const boost::shared_ptr<base_logger>& logger);
	void enable(bool value);
	std::string as_json() const;

	/* --- feeding statistics with collected data --- */

	// cache statistics
	void update_used_cache_size(size_t used_cache_size);

	// messages statistics from specific handle
	void update_handle_stats(const std::string& service,
							 const std::string& handle,
							 const handle_stats& stats);

	bool get_handle_stats(const std::string& service,
						  const std::string& handle,
						  handle_stats& stats);

	void update_service_stats(const std::string& service_name,
							  const service_stats& stats);

private:
	void init();
	void process_remote_connection();
	std::string all_services_json();
	std::string process_request_json(const std::string& request_json);


	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config() const;

	/* --- collected data --- */
	size_t used_cache_size_;

	// services status
	services_stats_t services_stats_;

	// handles status and statistics
	handle_stats_t handles_stats_;

private:
	bool is_enabled_;

	// global configuration object
	boost::shared_ptr<configuration> config_;

	// logger
	boost::shared_ptr<base_logger> logger_;

	// zmq context
	boost::shared_ptr<zmq::context_t> zmq_context_;

	boost::thread thread_;
	boost::mutex mutex_;
	bool is_running_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_STATISTICS_COLLECTOR_HPP_INCLUDED_
