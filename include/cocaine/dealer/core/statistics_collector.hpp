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

enum statictics_req_error {
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
	statistics_collector(boost::shared_ptr<configuration> config,
						 boost::shared_ptr<zmq::context_t> context);

	statistics_collector(boost::shared_ptr<configuration> config,
						 boost::shared_ptr<zmq::context_t> context,
						 boost::shared_ptr<base_logger> logger);

	virtual ~statistics_collector();
	
	std::string get_error_json(enum statictics_req_error err) const;

	void set_logger(boost::shared_ptr<base_logger> logger);
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
	std::string cache_stats_json() const;
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
