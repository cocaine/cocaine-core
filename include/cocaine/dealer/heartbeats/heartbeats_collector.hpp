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

#ifndef _COCAINE_DEALER_HEARTBEATS_COLLECTOR_HPP_INCLUDED_
#define _COCAINE_DEALER_HEARTBEATS_COLLECTOR_HPP_INCLUDED_

#include <string>
#include <map>
#include <set>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>

#include "json/json.h"

#include <zmq.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/defaults.hpp"
#include "cocaine/dealer/core/configuration.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"
#include "cocaine/dealer/utils/refresher.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/heartbeats/hosts_fetcher_iface.hpp"
#include "cocaine/dealer/core/handle_info.hpp"
#include "cocaine/dealer/core/inetv4_host.hpp"
#include "cocaine/dealer/cocaine_node_info/cocaine_node_info.hpp"
#include "cocaine/dealer/core/cocaine_endpoint.hpp"

namespace cocaine {
namespace dealer {

class heartbeats_collector : private boost::noncopyable {
public:
	typedef std::map<std::string, std::vector<cocaine_endpoint> > handles_endpoints_t;
	typedef boost::function<void(const service_info_t&, const handles_endpoints_t&)> callback_t;

	heartbeats_collector(boost::shared_ptr<configuration> config,
						 boost::shared_ptr<zmq::context_t> zmq_context);

	virtual ~heartbeats_collector();

	void run();
	void stop();

	void set_logger(boost::shared_ptr<base_logger> logger);
	void set_callback(callback_t callback);
	
private:
	typedef boost::shared_ptr<hosts_fetcher_iface> hosts_fetcher_ptr;
	typedef hosts_fetcher_iface::inetv4_endpoints inetv4_endpoints;

	void ping_services();
	void ping_endpoints();
	void process_alive_endpoints();

	bool get_metainfo_from_endpoint(const inetv4_endpoint& endpoint, std::string& response);

	void log_responded_hosts_handles(const service_info_t& service_info,
									 const handles_endpoints_t& handles_endpoints);

	static const int hosts_ping_timeout = 2000; // milliseconds
	static const int host_socket_ping_timeout = 2000000; // (2 seconds) microseconds FIX in zmq 3.1

private:
	boost::shared_ptr<configuration> config_;
	boost::shared_ptr<zmq::context_t> zmq_context_;
	boost::shared_ptr<base_logger> logger_;
	std::vector<hosts_fetcher_ptr> hosts_fetchers_;

	// endpoints cache
	std::map<std::string, inetv4_endpoints> services_endpoints_;
	std::set<inetv4_endpoint> all_endpoints_;
	std::map<inetv4_endpoint, cocaine_node_info_t> endpoints_metadata_;

	std::auto_ptr<refresher> refresher_;
	callback_t callback_;

	// synchronization
	boost::mutex mutex_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HEARTBEATS_COLLECTOR_HPP_INCLUDED_
