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

#ifndef _COCAINE_DEALER_HTTP_HEARTBEATS_COLLECTOR_HPP_INCLUDED_
#define _COCAINE_DEALER_HTTP_HEARTBEATS_COLLECTOR_HPP_INCLUDED_

#include <memory>
#include <string>
#include <map>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/bind.hpp>

#include <zmq.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/details/smart_logger.hpp"
#include "cocaine/dealer/details/refresher.hpp"
#include "cocaine/dealer/details/heartbeats_collector.hpp"
#include "cocaine/dealer/details/curl_hosts_fetcher.hpp"
#include "cocaine/dealer/details/configuration.hpp"

namespace cocaine {
namespace dealer {
	
class http_heartbeats_collector : public heartbeats_collector, private boost::noncopyable {
public:
	http_heartbeats_collector(boost::shared_ptr<configuration> config,
							  boost::shared_ptr<zmq::context_t> zmq_context);

	virtual ~http_heartbeats_collector();

	void run();
	void stop();

	void set_logger(boost::shared_ptr<base_logger> logger);
	void set_callback(heartbeats_collector::callback_t callback);
	
private:
	void hosts_callback(std::vector<cocaine::dealer::host_info_t>& hosts, service_info_t tag);
	void services_ping_callback();
	void ping_service_hosts(const service_info_t& s_info, std::vector<host_info_t>& hosts);

	void parse_host_response(const service_info_t& s_info,
							 DT::ip_addr ip,
							 const std::string& response,
							 std::vector<handle_info_t>& handles);

	void validate_host_handles(const service_info_t& s_info,
							   const std::vector<host_info_t>& hosts,
							   const std::multimap<DT::ip_addr, handle_info_t>& hosts_and_handles) const;

	bool get_metainfo_from_host(const service_info_t& s_info,
								DT::ip_addr ip,
								std::string& response);

	static const int curl_fetcher_timeout = 1;
	static const int hosts_ping_timeout = 1;

private:
	boost::shared_ptr<configuration> config_;
	boost::shared_ptr<zmq::context_t> zmq_context_;
	boost::shared_ptr<base_logger> logger_;

	typedef std::map<std::string, std::vector<host_info_t> > service_hosts_map;

	std::vector<boost::shared_ptr<curl_hosts_fetcher> > hosts_fetchers_;
	service_hosts_map fetched_services_hosts_;
	std::auto_ptr<refresher> refresher_;

	heartbeats_collector::callback_t callback_;
	boost::mutex mutex_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HTTP_HEARTBEATS_COLLECTOR_HPP_INCLUDED_
