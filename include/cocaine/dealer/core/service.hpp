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
#include "cocaine/dealer/core/dealer_object.hpp"
#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/core/cached_response.hpp"
#include "cocaine/dealer/core/cocaine_endpoint.hpp"

#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"
#include "cocaine/dealer/utils/refresher.hpp"

#include "cocaine/dealer/storage/eblob.hpp"

namespace cocaine {
namespace dealer {

class service_t : private boost::noncopyable, public dealer_object_t {
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

	// registered response_t callback 
	typedef std::map<std::string, boost::weak_ptr<response_t> > registered_callbacks_map_t;

	typedef std::map<std::string, std::vector<cocaine_endpoint_t> > handles_endpoints_t;

public:
	service_t(const service_info_t& info,
			  const boost::shared_ptr<context_t>& ctx,
			  bool logging_enabled = true);

	virtual ~service_t();

	void refresh_handles(const handles_endpoints_t& handles_endpoints);

	void send_message(cached_message_prt_t message);
	bool is_dead();

	service_info_t info() const;

	void register_responder_callback(const std::string& message_uuid,
									 const boost::shared_ptr<response_t>& response_t);

	void unregister_responder_callback(const std::string& message_uuid);

private:
	void create_new_handles(const handles_info_list_t& handles_info,
							const handles_endpoints_t& handles_endpoints);

	void create_handle(const handle_info_t& handle_info,
					   const handles_endpoints_t& handles_endpoints);

	void remove_outstanding_handles(const handles_info_list_t& handles_info);
	void update_existing_handles(const handles_endpoints_t& handles_endpoints);

	void enqueue_responce(cached_response_prt_t response_t);
	void dispatch_responces();
	bool responces_queues_empty() const;

	void check_for_deadlined_messages();

	bool enque_to_handle(const cached_message_prt_t& message);
	void enque_to_unhandled(const cached_message_prt_t& message);
	
	void append_to_unhandled(const std::string& handle_name,
							 const messages_deque_ptr_t& handle_queue);

	void get_outstanding_handles(const handles_endpoints_t& handles_endpoints,
								 handles_info_list_t& outstanding_handles);

	void get_new_handles(const handles_endpoints_t& handles_endpoints,
						 handles_info_list_t& new_handles);

	void destroy_handle(const std::string& handle_name);

	messages_deque_ptr_t get_and_remove_unhandled_queue(const std::string& handle_name);

private:
	// service information
	service_info_t m_info;

	// handles map (handle name, handle ptr)
	handles_map_t m_handles;

	// service messages for non-existing handles <handle name, handle ptr>
	unhandled_messages_map_t m_unhandled_messages;

	// responces map <handle name, responces queue ptr>
	responces_map_t m_received_responces;

	boost::thread				m_thread;
	boost::mutex				m_responces_mutex;
	boost::mutex				m_handles_mutex;
	boost::mutex				m_unhandled_mutex;
	boost::condition_variable	m_cond_var;

	volatile bool m_is_running;

	// responses callbacks
	registered_callbacks_map_t m_responses_callbacks_map;

	// deadlined messages refresher
	std::auto_ptr<refresher> m_deadlined_messages_refresher;

	static const int deadline_check_interval = 1000; // millisecs

	bool m_is_dead;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SERVICE_HPP_INCLUDED_
