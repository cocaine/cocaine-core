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

#include "cocaine/dealer/core/service.hpp"

namespace cocaine {
namespace dealer {

service_t::service_t(const service_info_t& info, boost::shared_ptr<cocaine::dealer::context> context) :
	info_(info),
	context_(context),
	is_running_(false),
	is_dead_(false)
{
	// run response dispatch thread
	is_running_ = true;
	thread_ = boost::thread(&service_t::dispatch_responces, this);

	// run timed out messages checker
	deadlined_messages_refresher_.reset(new refresher(boost::bind(&service_t::check_for_deadlined_messages, this), deadline_check_interval));
}

service_t::~service_t() {
	is_dead_ = true;

	// kill handles
	handles_map_t::iterator it = handles_.begin();
	for (;it != handles_.end(); ++it) {
		it->second->disconnect();
	}

	is_running_ = false;
	cond_.notify_one();
	thread_.join();
}

bool
service_t::is_dead() {
	return is_dead_;
}

service_info_t
service_t::info() const {
	return info_;
}

bool
service_t::responces_queues_empty() const {
	responces_map_t::const_iterator it = received_responces_.begin();
	for (; it != received_responces_.end(); ++it) {
		responces_deque_ptr_t handle_resp_queue = it->second;
		if (!handle_resp_queue->empty()) {
			return false;
		}
	}

	return true;
}

void
service_t::dispatch_responces() {
	while (is_running_) {
		boost::mutex::scoped_lock lock(mutex_);

		while(responces_queues_empty() && is_running_) {
            cond_.wait(lock);
        }

		// go through each response queue
		responces_map_t::iterator qit = received_responces_.begin();
		for (; qit != received_responces_.end(); ++qit) {

			// get first responce from queue
			responces_deque_ptr_t handle_resp_queue = qit->second;

			if (!handle_resp_queue->empty()) {
				cached_response_prt_t resp_ptr = handle_resp_queue->front();

				// create simplified response
				response_data resp_data;
				resp_data.data = resp_ptr->data().data();
				resp_data.size = resp_ptr->data().size();

				response_info resp_info;
				resp_info.uuid = resp_ptr->uuid();
				resp_info.path = resp_ptr->path();
				resp_info.code = resp_ptr->code();
				resp_info.error_msg = resp_ptr->error_message();

				// invoke callback for given message uuid
				try {
					registered_callbacks_map_t::iterator it = responses_callbacks_map_.find(resp_info.uuid);

					// call callback it it's there
					if (it != responses_callbacks_map_.end()) {
						boost::weak_ptr<response> response_wptr = it->second;
						boost::shared_ptr<response> response_ptr = response_wptr.lock();
						
						lock.unlock();
						
						if (!response_ptr) {
							unregister_responder_callback(resp_ptr->uuid());
						}
						else {
							response_ptr->response_callback(resp_data, resp_info);
						}
					}
				}
				catch (...) {
				}

				lock.lock();

				// remove processed response
				handle_resp_queue->pop_front();
			}
		}
	}
}

boost::shared_ptr<cocaine::dealer::context>
service_t::context() {
	if (!context_.get()) {
		throw internal_error("dealer context object is empty at " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return context_;
}

boost::shared_ptr<base_logger>
service_t::logger() {
	return context()->logger();
}

boost::shared_ptr<configuration>
service_t::config() {
	boost::shared_ptr<configuration> conf = context()->config();
	if (!conf.get()) {
		throw internal_error("configuration object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return conf;
}

void
service_t::log_refreshed_endpoints(const handles_endpoints_t& endpoints) {
	/*
	logger()->log(PLOG_INFO, "service %s refreshed with: ", info_.name_.c_str());

	for (size_t i = 0; i < hosts.size(); ++i) {
		std::stringstream tmp;
		tmp << "host - " << hosts[i];
		logger()->log(PLOG_DEBUG, tmp.str());
	}

	for (size_t i = 0; i < handles.size(); ++i) {
		std::stringstream tmp;
		tmp << "handle - " << handles[i].as_string();
		logger()->log(PLOG_DEBUG, tmp.str());
	}
	*/
}

void
service_t::register_responder_callback(const std::string& message_uuid,
											const boost::shared_ptr<response>& resp)
{
	boost::mutex::scoped_lock lock(mutex_);
	boost::weak_ptr<response> wptr(resp);
	responses_callbacks_map_[message_uuid] = wptr;
}

void
service_t::unregister_responder_callback(const std::string& message_uuid) {
	boost::mutex::scoped_lock lock(mutex_);
	registered_callbacks_map_t::iterator callback_it = responses_callbacks_map_.find(message_uuid);

	// is there a callback for given response uuid?
	if (callback_it == responses_callbacks_map_.end()) {
		return;
	}

	responses_callbacks_map_.erase(callback_it);
}

void
service_t::enqueue_responce(cached_response_prt_t response) {
	boost::mutex::scoped_lock lock(mutex_);

	// validate response
	assert(response);

	const message_path& path = response->path();

	// see whether there exists registered callback for message
	registered_callbacks_map_t::iterator callback_it = responses_callbacks_map_.find(response->uuid());

	// is there a callback for given response uuid?
	if (callback_it == responses_callbacks_map_.end()) {
		// drop response
		//lock.unlock();
		return;
	}

	// get responces queue for response handle
	responces_map_t::iterator it = received_responces_.find(path.handle_name);
	responces_deque_ptr_t handle_resp_queue;

	// if no queue for handle's responces exists, create one
	if (it == received_responces_.end()) {
		handle_resp_queue.reset(new cached_responces_deque_t);
		received_responces_.insert(std::make_pair(path.handle_name, handle_resp_queue));
	}
	else {
		handle_resp_queue = it->second;

		// validate existing responces queue
		if (!handle_resp_queue) {
			std::string error_str = "found empty response queue object!";
			error_str += " service: " + info_.name_ + " handle: " + path.handle_name;
			error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw internal_error(error_str);
		}
	}

	// add responce to queue
	handle_resp_queue->push_back(response);
	lock.unlock();
	cond_.notify_one();
}

void
service_t::refresh_handles(const handles_endpoints_t& handles_endpoints) {
	boost::mutex::scoped_lock lock(mutex_);

	handles_info_list_t outstanding_handles;
	handles_info_list_t new_handles;

	// check for outstanding handles
	for (handles_map_t::iterator it = handles_.begin(); it != handles_.end(); ++it) {
		handles_endpoints_t::const_iterator hit = handles_endpoints.find(it->first);

		if (hit == handles_endpoints.end()) {
			outstanding_handles.push_back(it->second->info());
		}
	}

	// check for new handles
	handles_endpoints_t::const_iterator it = handles_endpoints.begin();
	for (; it != handles_endpoints.end(); ++it) {
		handles_map_t::iterator hit = handles_.find(it->first);

		if (hit == handles_.end()) {
			handle_info_t hinfo(it->first, info_.app_, info_.name_);
			new_handles.push_back(hinfo);
		}
	}

	lock.unlock();
	remove_outstanding_handles(outstanding_handles);
	update_existing_handles(handles_endpoints);
	create_new_handles(new_handles, handles_endpoints);
}

void
service_t::update_existing_handles(const handles_endpoints_t& handles_endpoints) {
	boost::mutex::scoped_lock lock(mutex_);

	handles_map_t::iterator it = handles_.begin();
	for (; it != handles_.end(); ++it) {

		handles_endpoints_t::const_iterator eit = handles_endpoints.find(it->first);

		if (eit != handles_endpoints.end()) {
			handle_ptr_t handle = it->second;

			lock.unlock();
			handle->update_endpoints(eit->second);
			lock.lock();
		}
	}
}

void
service_t::verify_unhandled_msg_queue_for_handle(const handle_ptr_t& handle) {
	// find corresponding unhandled message queue
	unhandled_messages_map_t::iterator it = unhandled_messages_.find(handle->info().name_);

	if (it == unhandled_messages_.end()) {
		messages_deque_ptr_t new_queue(new cached_messages_deque_t);
		unhandled_messages_[handle->info().name_] = new_queue;
		return;
	}

	// should not find a queue with messages!
	messages_deque_ptr_t msg_queue = it->second;

	if (msg_queue && !msg_queue->empty()) {
		std::string error_str = "found unhandled non-empty message queue with existing handle for ";
		error_str += " handle " + handle->description();
		error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}
}

void
service_t::remove_outstanding_handles(const handles_info_list_t& handles) {
	boost::mutex::scoped_lock lock(mutex_);

	// no handles to destroy
	if (handles.empty()) {
		return;
	}

	std::string message_str = "service: [" + info_.name_ + "] is removing outstanding handles: ";
	for (size_t i = 0; i < handles.size(); ++i) {
		message_str += handles[i].name_;

		if (i != handles.size() - 1) {
			message_str += ", ";
		}
	}

	logger()->log(message_str);

	// destroy handles
	for (size_t i = 0; i < handles.size(); ++i) {
		handles_map_t::iterator it = handles_.find(handles[i].name_);

		if (it != handles_.end()) {
			handle_ptr_t handle = it->second;

			// immediately terminate all handle activity
			lock.unlock();
			handle->disconnect();
			lock.lock();

			verify_unhandled_msg_queue_for_handle(handle);

			// consolidate all handle's messages
			handle->make_all_messages_new();

			// move handle messages to unhandled messages map in service
			messages_deque_ptr_t handle_msg_queue = handle->new_messages();

			// validate handle queue
			assert(handle_msg_queue);

			// in case there are messages, store them
			if (!handle_msg_queue->empty()) {
				logger()->log(PLOG_DEBUG, "moving message queue from handle %s to service, queue size: %d",
							  handle->description().c_str(), handle_msg_queue->size());

				messages_deque_ptr_t unhandled_queue = unhandled_messages_[handle->info().name_];
				unhandled_queue->insert(unhandled_queue->end(), handle_msg_queue->begin(), handle_msg_queue->end());
			}
		}

		handles_.erase(it);
	}
}

void
service_t::create_new_handles(const handles_info_list_t& handles,
							  const handles_endpoints_t& handles_endpoints)
{
	boost::mutex::scoped_lock lock(mutex_);

	// no handles to create
	if (handles.empty()) {
		return;
	}

	// create handles
	for (size_t i = 0; i < handles.size(); ++i) {
		handle_ptr_t handle;
		handle_info_t handle_info = handles[i];

		handles_endpoints_t::const_iterator hit = handles_endpoints.find(handle_info.name_);
		if (hit == handles_endpoints.end()) {
			throw internal_error("no endpoints for new handle " + handle_info.as_string() + "].");
		}

		lock.unlock();
		handle.reset(new dealer::handle_t(handle_info, context_, hit->second));
		lock.lock();

		// set responce callback
		typedef dealer::handle_t::responce_callback_t resp_callback;
		resp_callback callback = boost::bind(&service_t::enqueue_responce, this, _1);

		lock.unlock();
		handle->set_responce_callback(callback);
		lock.lock();

		// move existing unhandled message queue to handle
		unhandled_messages_map_t::iterator it = unhandled_messages_.find(handles[i].name_);
		if (it != unhandled_messages_.end()) {
			messages_deque_ptr_t msg_queue = it->second;

			if (msg_queue && !msg_queue->empty()) {
				lock.unlock();
				logger()->log(PLOG_DEBUG, "assign unhandled message queue to handle %s, queue size: %d",
							  handle->description().c_str(), msg_queue->size());
				handle->assign_message_queue(it->second);
				lock.lock();
			}

			unhandled_messages_.erase(it);
		}

		handles_[handles[i].name_] = handle;
	}
}

void
service_t::send_message(cached_message_prt_t message) {
	boost::mutex::scoped_lock lock(mutex_);

	const std::string& handle_name = message->path().handle_name;

	// find existing handle to enqueue message
	handles_map_t::iterator it = handles_.find(handle_name);
	if (it != handles_.end()) {
		handle_ptr_t handle = it->second;

		// make sure we have valid handle
		lock.unlock();
		assert(handle);
		handle->enqueue_message(message);
		lock.lock();
	}
	else {
		// if no handle, store locally
		unhandled_messages_map_t::iterator it = unhandled_messages_.find(handle_name);

		// check for existing messages queue for handle
		messages_deque_ptr_t queue;

		if (it == unhandled_messages_.end()) {
			queue.reset(new cached_messages_deque_t);
			queue->push_back(message);
			unhandled_messages_[handle_name] = queue;
		}
		else {
			queue = it->second;
			assert(queue);
			queue->push_back(message);
		}
	}
}

void
service_t::check_for_deadlined_messages() {
	boost::mutex::scoped_lock lock(mutex_);

	unhandled_messages_map_t::iterator it = unhandled_messages_.begin();

	for (; it != unhandled_messages_.end(); ++it) {
		messages_deque_ptr_t queue = it->second;
		cached_messages_deque_t::iterator qit = queue->begin();

		// create tmp queue
		messages_deque_ptr_t not_expired_queue(new cached_messages_deque_t);
		messages_deque_ptr_t expired_queue(new cached_messages_deque_t);
		bool found_expired = false;

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

		// create error response for deadlined message
		cached_messages_deque_t::iterator expired_qit = expired_queue->begin();

		for (;expired_qit != expired_queue->end(); ++expired_qit) {
			cached_response_prt_t response;
			response.reset(new cached_response_t((*expired_qit)->uuid(),
											   (*expired_qit)->path(),
											   deadline_error,
											   "message expired"));

			lock.unlock();
			enqueue_responce(response);
			lock.lock();
		}
	}
}

} // namespace dealer
} // namespace cocaine
