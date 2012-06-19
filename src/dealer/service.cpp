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

#include "cocaine/dealer/core/service.hpp"

namespace cocaine {
namespace dealer {

service_t::service_t(const service_info_t& info,
					 const boost::shared_ptr<context_t>& ctx,
					 bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled),
	m_info(info),
	m_is_running(false),
	m_is_dead(false)
{
	// run response_t dispatch thread
	m_is_running = true;
	m_thread = boost::thread(&service_t::dispatch_responces, this);

	// run timed out messages checker
	m_deadlined_messages_refresher.reset(new refresher(boost::bind(&service_t::check_for_deadlined_messages, this),
										 deadline_check_interval));
}

service_t::~service_t() {
	m_is_dead = true;

	log(PLOG_INFO, "FINISH SERVICE [%s]", m_info.name.c_str());

	// kill handles
	handles_map_t::iterator it = m_handles.begin();
	for (;it != m_handles.end(); ++it) {
		
		log(PLOG_INFO,
		"DESTROY HANDLE [%s.%s.%s]",
		m_info.name.c_str(),
		m_info.app.c_str(),
		it->second->info().name.c_str());

		it->second->disconnect();
	}

	m_is_running = false;
	m_cond_var.notify_one();
	m_thread.join();
}

bool
service_t::is_dead() {
	return m_is_dead;
}

service_info_t
service_t::info() const {
	return m_info;
}

bool
service_t::responces_queues_empty() const {
	responces_map_t::const_iterator it = m_received_responces.begin();
	for (; it != m_received_responces.end(); ++it) {
		responces_deque_ptr_t handle_resp_queue = it->second;
		if (!handle_resp_queue->empty()) {
			return false;
		}
	}

	return true;
}

void
service_t::dispatch_responces() {
	while (m_is_running) {
		boost::mutex::scoped_lock lock(m_responces_mutex);

		while(responces_queues_empty() && m_is_running) {
            m_cond_var.wait(lock);
        }

		// go through each response_t queue
		responces_map_t::iterator qit = m_received_responces.begin();
		for (; qit != m_received_responces.end(); ++qit) {

			// get first responce from queue
			responces_deque_ptr_t handle_resp_queue = qit->second;

			if (!handle_resp_queue->empty()) {
				cached_response_prt_t resp_ptr = handle_resp_queue->front();

				// create simplified response_t
				response_data resp_data;
				resp_data.data = resp_ptr->data().data();
				resp_data.size = resp_ptr->data().size();

				response_info resp_info;
				resp_info.uuid = resp_ptr->uuid();
				resp_info.path = resp_ptr->path();
				resp_info.code = resp_ptr->code();
				resp_info.error_msg = resp_ptr->error_message();

				// invoke callback for given message uuid
				bool unlocked = true;
				try {
					registered_callbacks_map_t::iterator it = m_responses_callbacks_map.find(resp_info.uuid);

					// call callback it it's there
					if (it != m_responses_callbacks_map.end()) {
						boost::weak_ptr<response_t> response_wptr = it->second;
						boost::shared_ptr<response_t> response_ptr = response_wptr.lock();
						
						lock.unlock();
						unlocked = true;
						
						if (!response_ptr) {
							unregister_responder_callback(resp_ptr->uuid());
						}
						else {
							response_ptr->response_callback(resp_data, resp_info);
						}
					}
				}
				catch (const std::exception& ex) {
					log(PLOG_ERROR, "could not process response: %s", ex.what());
				}

				if (unlocked) {
					lock.lock();
				}

				// remove processed response_t
				handle_resp_queue->pop_front();
			}
		}
	}
}

void
service_t::register_responder_callback(const std::string& message_uuid,
											const boost::shared_ptr<response_t>& resp)
{
	boost::mutex::scoped_lock lock(m_responces_mutex);
	boost::weak_ptr<response_t> wptr(resp);
	m_responses_callbacks_map[message_uuid] = wptr;
}

void
service_t::unregister_responder_callback(const std::string& message_uuid) {
	boost::mutex::scoped_lock lock(m_responces_mutex);
	registered_callbacks_map_t::iterator callback_it = m_responses_callbacks_map.find(message_uuid);

	// is there a callback for given response_t uuid?
	if (callback_it == m_responses_callbacks_map.end()) {
		return;
	}

	m_responses_callbacks_map.erase(callback_it);
}

void
service_t::enqueue_responce(cached_response_prt_t response_t) {
	// validate response_t
	assert(response_t);

	const message_path_t& path = response_t->path();

	// see whether there exists registered callback for message
	boost::mutex::scoped_lock lock(m_responces_mutex);
	registered_callbacks_map_t::iterator callback_it = m_responses_callbacks_map.find(response_t->uuid());

	// is there a callback for given response_t uuid?
	if (callback_it == m_responses_callbacks_map.end()) {
		// drop response
		return;
	}

	// get responces queue for response_t handle
	responces_map_t::iterator it = m_received_responces.find(path.handle_name);
	responces_deque_ptr_t handle_resp_queue;

	// if no queue for handle's responces exists, create one
	if (it == m_received_responces.end()) {
		handle_resp_queue.reset(new cached_responces_deque_t);
		m_received_responces.insert(std::make_pair(path.handle_name, handle_resp_queue));
	}
	else {
		handle_resp_queue = it->second;

		// validate existing responces queue
		if (!handle_resp_queue) {
			std::string error_str = "found empty response_t queue object!";
			error_str += " service: " + m_info.name + " handle: " + path.handle_name;
			error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw internal_error(error_str);
		}
	}

	// add responce to queue
	handle_resp_queue->push_back(response_t);
	lock.unlock();
	m_cond_var.notify_one();
}

bool
service_t::enque_to_handle(const cached_message_prt_t& message) {
	boost::mutex::scoped_lock lock(m_handles_mutex);

	handles_map_t::iterator it = m_handles.find(message->path().handle_name);
	if (it == m_handles.end()) {
		return false;
	}

	handle_ptr_t handle = it->second;
	assert(handle);
	handle->enqueue_message(message);

	return true;
}

void
service_t::enque_to_unhandled(const cached_message_prt_t& message) {
	boost::mutex::scoped_lock lock(m_unhandled_mutex);

	const std::string& handle_name = message->path().handle_name;
	unhandled_messages_map_t::iterator it = m_unhandled_messages.find(handle_name);
	
	// check for existing message queue for handle
	if (it == m_unhandled_messages.end()) {
		messages_deque_ptr_t queue(new cached_messages_deque_t);
		queue->push_back(message);
		m_unhandled_messages[handle_name] = queue;
	}
	else {
		messages_deque_ptr_t queue = it->second;
		assert(queue);
		queue->push_back(message);
	}
}

boost::shared_ptr<std::deque<boost::shared_ptr<message_iface> > >
service_t::get_and_remove_unhandled_queue(const std::string& handle_name) {
	boost::mutex::scoped_lock lock(m_unhandled_mutex);

	messages_deque_ptr_t queue(new cached_messages_deque_t);

	unhandled_messages_map_t::iterator it = m_unhandled_messages.find(handle_name);
	if (it == m_unhandled_messages.end()) {
		return queue;
	}

	queue = it->second;
	m_unhandled_messages.erase(it);

	return queue;
}

void
service_t::send_message(cached_message_prt_t message) {
	bool enqued = enque_to_handle(message);

	if (!enqued) {
		enque_to_unhandled(message);
	}
}

void
service_t::destroy_handle(const std::string& handle_name) {
	boost::mutex::scoped_lock lock(m_handles_mutex);

	handles_map_t::iterator it = m_handles.find(handle_name);

	if (it == m_handles.end()) {
		log(PLOG_ERROR, "unable to DESTROY HANDLE [%s], handle object missing.", handle_name.c_str());
		return;
	}

	handle_ptr_t handle = it->second;
	assert(handle);

	log(PLOG_WARNING, "DESTROY HANDLE [%s]", handle_name.c_str());

	// retrieve message cache and terminate all handle activity
	handle->kill();

	boost::shared_ptr<message_cache_t> mcache = handle->messages_cache();
	
	log(PLOG_DEBUG, "messages cache - start");
	//mcache->log_stats();

	mcache->make_all_messages_new();

	log(PLOG_DEBUG, "messages cache - end");
	mcache->log_stats();

	const messages_deque_ptr_t& handle_queue = mcache->new_messages();
	
	log(PLOG_DEBUG, "handle_queue size: %d", handle_queue->size());

	append_to_unhandled(handle_name, handle_queue);

	m_handles.erase(it);
	lock.unlock();

	log(PLOG_DEBUG, "DESTROY HANDLE [%s] DONE", handle_name.c_str());

	size_t alive_count = 0;

	registered_callbacks_map_t::iterator it2 = m_responses_callbacks_map.begin();
	for (; it2 != m_responses_callbacks_map.end(); ++it2) {
		boost::weak_ptr<response_t> wp = it2->second;
		boost::shared_ptr<response_t> response_ptr = wp.lock();

		if (response_ptr) {
			++alive_count;
		}
	}

	log(PLOG_DEBUG, "callbacks count: %d, alive: %d", m_responses_callbacks_map.size(), alive_count);

	size_t respnces_count = 0;

	responces_map_t::iterator it3 = m_received_responces.begin();
	for (; it3 != m_received_responces.end(); ++it3) {
		respnces_count += it3->second->size();
	}

	log(PLOG_DEBUG, "responces enqued: %d", respnces_count);
}

void
service_t::append_to_unhandled(const std::string& handle_name,
							  const messages_deque_ptr_t& handle_queue)
{
	assert(handle_queue);

	if (handle_queue->empty()) {
		log(PLOG_DEBUG, "handle_queue->empty()");
		return;
	}

	// in case there are messages, store them		
	log(PLOG_DEBUG, "moving message queue from handle [%s.%s] to service, queue size: %d",
					m_info.name.c_str(),
					handle_name.c_str(),
					handle_queue->size());

	boost::mutex::scoped_lock lock(m_unhandled_mutex);

	// find corresponding unhandled message queue
	unhandled_messages_map_t::iterator it = m_unhandled_messages.find(handle_name);

	messages_deque_ptr_t queue;
	if (it == m_unhandled_messages.end()) {
		queue.reset(new cached_messages_deque_t);
		m_unhandled_messages[handle_name] = queue;
	}
	else {
		queue = it->second;
	}

	assert(queue);
	queue->insert(queue->end(), handle_queue->begin(), handle_queue->end());

	// clear metadata
	for (cached_messages_deque_t::iterator it = queue->begin(); it != queue->end(); ++it) {
		(*it)->mark_as_sent(false);
		(*it)->set_ack_received(false);
	}

	log(PLOG_DEBUG, "moving message queue done.");
}

void
service_t::get_outstanding_handles(const handles_endpoints_t& handles_endpoints,
								   handles_info_list_t& outstanding_handles)
{
	boost::mutex::scoped_lock lock(m_handles_mutex);

	for (handles_map_t::iterator it = m_handles.begin(); it != m_handles.end(); ++it) {
		const std::string& handle_name = it->first;

		handles_endpoints_t::const_iterator hit = handles_endpoints.find(handle_name);
		if (hit == handles_endpoints.end()) {
			outstanding_handles.push_back(it->second->info());
		}
	}
}

void
service_t::get_new_handles(const handles_endpoints_t& handles_endpoints,
						   handles_info_list_t& new_handles)
{
	boost::mutex::scoped_lock lock(m_handles_mutex);

	handles_endpoints_t::const_iterator it = handles_endpoints.begin();
	for (; it != handles_endpoints.end(); ++it) {
		const std::string& handle_name = it->first;

		handles_map_t::iterator hit = m_handles.find(handle_name);
		if (hit == m_handles.end()) {
			handle_info_t hinfo(handle_name, m_info.app, m_info.name);
			new_handles.push_back(hinfo);
		}
	}
}

void
service_t::refresh_handles(const handles_endpoints_t& handles_endpoints) {
	handles_info_list_t outstanding_handles;
	handles_info_list_t new_handles;

	get_outstanding_handles(handles_endpoints, outstanding_handles);
	get_new_handles(handles_endpoints, new_handles);

	remove_outstanding_handles(outstanding_handles);
	update_existing_handles(handles_endpoints);
	create_new_handles(new_handles, handles_endpoints);
}

void
service_t::remove_outstanding_handles(const handles_info_list_t& handles_info) {
	if (handles_info.empty()) {
		return;
	}

	for (size_t i = 0; i < handles_info.size(); ++i) {
		destroy_handle(handles_info[i].name);
	}
}

void
service_t::update_existing_handles(const handles_endpoints_t& handles_endpoints) {
	boost::mutex::scoped_lock lock(m_handles_mutex);

	handles_map_t::iterator it = m_handles.begin();
	for (; it != m_handles.end(); ++it) {

		handles_endpoints_t::const_iterator eit = handles_endpoints.find(it->first);

		if (eit != handles_endpoints.end()) {
			handle_ptr_t handle = it->second;
			handle->update_endpoints(eit->second);
		}
	}
}

void
service_t::create_handle(const handle_info_t& handle_info,
						 const handles_endpoints_t& handles_endpoints)
{
	const std::string& handle_name = handle_info.name;

	log(PLOG_INFO,
		"CREATE HANDLE [%s.%s.%s]",
		m_info.name.c_str(),
		m_info.app.c_str(),
		handle_name.c_str());

	// create new handle
	handles_endpoints_t::const_iterator it = handles_endpoints.find(handle_name);
	if (it == handles_endpoints.end()) {
		throw internal_error("no endpoints for new handle " + handle_info.as_string());
	}

	handle_ptr_t handle(new dealer::handle_t(handle_info, it->second, context()));
	handle->set_responce_callback(boost::bind(&service_t::enqueue_responce, this, _1));

	// retrieve unhandled queue
	messages_deque_ptr_t queue = get_and_remove_unhandled_queue(handle_name);

	if (!queue->empty()) {
		handle->assign_message_queue(queue);

		log(PLOG_DEBUG,
			"assign unhandled message queue to handle %s, queue size: %d",
			handle_info.as_string().c_str(),
			queue->size());
	}
	else {
		log(PLOG_DEBUG,
			"no unhandled message queue for handle %s",
			handle_info.as_string().c_str());
	}

	// append new handle
	boost::mutex::scoped_lock lock(m_handles_mutex);	
	m_handles[handle_name] = handle;
}

void
service_t::create_new_handles(const handles_info_list_t& handles_info,
							  const handles_endpoints_t& handles_endpoints)
{
	// no handles to create
	if (handles_info.empty()) {
		return;
	}

	// create handles from info and endpoints
	for (size_t i = 0; i < handles_info.size(); ++i) {
		create_handle(handles_info[i], handles_endpoints);
	}
}

void
service_t::check_for_deadlined_messages() {
	boost::mutex::scoped_lock lock(m_unhandled_mutex);

	unhandled_messages_map_t::iterator it = m_unhandled_messages.begin();

	for (; it != m_unhandled_messages.end(); ++it) {
		messages_deque_ptr_t queue = it->second;

		// create tmp queue
		messages_deque_ptr_t not_expired_queue(new cached_messages_deque_t);
		messages_deque_ptr_t expired_queue(new cached_messages_deque_t);
		bool found_expired = false;

		cached_messages_deque_t::iterator qit = queue->begin();
		for (;qit != queue->end(); ++qit) {
			if ((*qit)->is_expired()) {
				expired_queue->push_back(*qit);
				found_expired = true;
			}
			else {
				not_expired_queue->push_back(*qit);
			}
		}

		if (!found_expired) {
			continue;
		}

		it->second = not_expired_queue;

		// create error response_t for deadlined message
		cached_messages_deque_t::iterator expired_qit = expired_queue->begin();

		for (;expired_qit != expired_queue->end(); ++expired_qit) {
			cached_response_prt_t response_t;
			response_t.reset(new cached_response_t((*expired_qit)->uuid(),
												 "",
												 (*expired_qit)->path(),
												 deadline_error,
												 "message expired"));

			log(PLOG_ERROR, "deadline policy exceeded, for message " + (*expired_qit)->uuid());
			enqueue_responce(response_t);
		}
	}
}

} // namespace dealer
} // namespace cocaine
