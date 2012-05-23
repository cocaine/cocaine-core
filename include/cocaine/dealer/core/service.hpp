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

#ifndef _COCAINE_DEALER_SERVICE_HPP_INCLUDED_
#define _COCAINE_DEALER_SERVICE_HPP_INCLUDED_

#include <string>
#include <sstream>
#include <memory>
#include <map>
#include <vector>
#include <list>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/thread/thread.hpp>
#include <boost/function.hpp>

#include "cocaine/dealer/response.hpp"

#include "cocaine/dealer/core/handle.hpp"
#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/core/handle_info.hpp"
#include "cocaine/dealer/core/service_info.hpp"
#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/core/cached_response.hpp"
#include "cocaine/dealer/core/cocaine_endpoint.hpp"

#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"
#include "cocaine/dealer/utils/refresher.hpp"

#include "cocaine/dealer/storage/eblob.hpp"

namespace cocaine {
namespace dealer {

class service_t : private boost::noncopyable {
public:
	typedef std::vector<handle_info_t> handles_info_list_t;

	typedef boost::shared_ptr<handle_t> handle_ptr_t;
	typedef std::map<std::string, handle_ptr_t> handles_map_t;

	typedef boost::shared_ptr<message_iface> cached_message_prt_t;
	typedef boost::shared_ptr<cached_response_t> cached_response_prt_t;

	typedef std::deque<cached_message_prt_t> cached_messages_deque_t;
	typedef std::deque<cached_response_prt_t> cached_responces_deque_t;

	typedef boost::shared_ptr<cached_messages_deque_t> messages_deque_ptr_t;
	typedef boost::shared_ptr<cached_responces_deque_t> responces_deque_ptr_t;

	// map <handle_name/handle's unprocessed messages deque>
	typedef std::map<std::string, messages_deque_ptr_t> unhandled_messages_map_t;

	// map <handle_name/handle's responces deque>
	typedef std::map<std::string, responces_deque_ptr_t> responces_map_t;

	// registered response callback 
	typedef std::map<std::string, boost::weak_ptr<response> > registered_callbacks_map_t;

	typedef std::map<std::string, std::vector<cocaine_endpoint> > handles_endpoints_t;

public:
	service_t(const service_info_t& info, boost::shared_ptr<cocaine::dealer::context> context);
	virtual ~service_t();

	void refresh_handles(const handles_endpoints_t& handles_endpoints);

	void send_message(cached_message_prt_t message);
	bool is_dead();

	service_info_t info() const;

	void register_responder_callback(const std::string& message_uuid, const boost::shared_ptr<response>& response);
	void unregister_responder_callback(const std::string& message_uuid);

private:
	void create_new_handles(const handles_info_list_t& handles, const handles_endpoints_t& handles_endpoints);
	void remove_outstanding_handles(const handles_info_list_t& handles);
	void update_existing_handles(const handles_endpoints_t& handles_endpoints);

	void enqueue_responce(cached_response_prt_t response);
	void dispatch_responces();
	bool responces_queues_empty() const;

	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config();
	boost::shared_ptr<cocaine::dealer::context> context();

	void check_for_deadlined_messages();

private:
	// service information
	service_info_t info_;

	// handles map (handle name, handle ptr)
	handles_map_t handles_;

	// service messages for non-existing handles <handle name, handle ptr>
	unhandled_messages_map_t unhandled_messages_;

	// responces map <handle name, responces queue ptr>
	responces_map_t received_responces_;

	// dealer context
	boost::shared_ptr<cocaine::dealer::context> context_;

	// statistics
	service_stats stats_;

	boost::thread thread_;
	boost::mutex mutex_;
	boost::condition_variable cond_;

	volatile bool is_running_;

	// responses callbacks
	registered_callbacks_map_t responses_callbacks_map_;

	// deadlined messages refresher
	std::auto_ptr<refresher> deadlined_messages_refresher_;

	static const int deadline_check_interval = 10; // millisecs

	bool is_dead_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SERVICE_HPP_INCLUDED_
