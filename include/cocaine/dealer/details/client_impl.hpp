//
// Copyright (C) 2011 Rim Zaidullin <creator@bash.org.ru>
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

#ifndef _LSD_CLIENT_IMPL_HPP_INCLUDED_
#define _LSD_CLIENT_IMPL_HPP_INCLUDED_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <zmq.hpp>

#include <boost/function.hpp>
#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/dealer/details/context.hpp"
#include "cocaine/dealer/details/service.hpp"
#include "cocaine/dealer/details/heartbeats_collector.hpp"

namespace lsd {

class client_impl : private boost::noncopyable {
public:
	explicit client_impl(const std::string& config_path = "");
	virtual ~client_impl();

	void connect();
	void disconnect();

	// send binary data
	std::string send_message(const void* data,
							 size_t size,
							 const std::string& service_name,
							 const std::string& handle_name);

	std::string send_message(const void* data,
							 size_t size,
							 const message_path& path);

	std::string send_message(const void* data,
							 size_t size,
							 const message_path& path,
							 const message_policy& policy);

	// send string data
	std::string send_message(const std::string& data,
							 const std::string& service_name,
							 const std::string& handle_name);

	std::string send_message(const std::string& data,
							 const message_path& path);

	std::string send_message(const std::string& data,
							 const message_path& path,
							 const message_policy& policy);

	int set_response_callback(boost::function<void(const response&, const response_info&)> callback,
							   const std::string& service_name,
							   const std::string& handle_name);

	size_t messages_cache_size() const;

	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config();
	boost::shared_ptr<lsd::context> context();

private:
	void update_messages_cache_size();
	void service_hosts_pinged_callback(const service_info_t& s_info, const std::vector<host_info_t>& hosts, const std::vector<handle_info_t>& handles);

private:
	typedef std::map<std::string, boost::shared_ptr<service_t> > services_map_t;

private:
	size_t messages_cache_size_;

	// main lsd context
	boost::shared_ptr<lsd::context> context_;

	// lsd service name mapped to service
	services_map_t services_;

	// heartsbeat collector
	std::auto_ptr<heartbeats_collector> heartbeats_collector_;

	// message response callback
	boost::function<void(const std::string&, void* data, size_t size)> response_callback_;

	// synchronization
	boost::mutex mutex_;
};

} // namespace lsd

#endif // _LSD_CLIENT_IMPL_HPP_INCLUDED_
