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
#include <map>
#include <vector>
#include <deque>

#include <zmq.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/thread/thread.hpp>
#include <boost/function.hpp>

#include "cocaine/dealer/structs.hpp"

#include "cocaine/dealer/details/error.hpp"
#include "cocaine/dealer/details/handle.hpp"
#include "cocaine/dealer/details/context.hpp"
#include "cocaine/dealer/details/host_info.hpp"
#include "cocaine/dealer/details/handle_info.hpp"
#include "cocaine/dealer/details/service_info.hpp"
#include "cocaine/dealer/details/smart_logger.hpp"
#include "cocaine/dealer/details/message_iface.hpp"
#include "cocaine/dealer/details/cached_response.hpp"

namespace cocaine {
namespace dealer {

// predeclaration
template <typename LSD_T> class service;
typedef service<DT> service_t;
template<typename T> std::ostream& operator << (std::ostream& out, const service<T>& s);

template <typename LSD_T>
class service : private boost::noncopyable {
public:
	typedef std::vector<host_info<LSD_T> > hosts_info_list_t;
	typedef std::vector<handle_info<LSD_T> > handles_info_list_t;

	typedef boost::shared_ptr<handle<LSD_T> > handle_ptr_t;
	typedef std::map<typename LSD_T::ip_addr, std::string> hosts_map_t;
	typedef std::map<std::string, handle_ptr_t> handles_map_t;

	typedef boost::shared_ptr<message_iface> cached_message_prt_t;
	typedef boost::shared_ptr<cached_response> cached_response_prt_t;

	typedef std::deque<cached_message_prt_t> cached_messages_deque_t;
	typedef std::deque<cached_response_prt_t> cached_responces_deque_t;

	typedef boost::shared_ptr<cached_messages_deque_t> messages_deque_ptr_t;
	typedef boost::shared_ptr<cached_responces_deque_t> responces_deque_ptr_t;

	// map <handle_name/handle's unprocessed messages deque>
	typedef std::map<std::string, messages_deque_ptr_t> unhandled_messages_map_t;

	// map <handle_name/handle's responces deque>
	typedef std::map<std::string, responces_deque_ptr_t> responces_map_t;

	// registered response callback
	typedef boost::function<void(const response&, const response_info&)> registered_callback_t;
	typedef std::map<std::string, registered_callback_t> registered_callbacks_map_t;

public:
	service(const service_info<LSD_T>& info, boost::shared_ptr<cocaine::dealer::context> context);
	virtual ~service();

	void refresh_hosts_and_handles(const hosts_info_list_t& hosts,
								   const std::vector<handle_info<LSD_T> >& handles);

	void send_message(cached_message_prt_t message);
	size_t cache_size() const;

	bool register_responder_callback(registered_callback_t callback,
									 const std::string& handle_name);

public:
	template<typename T> friend std::ostream& operator << (std::ostream& out, const service<T>& s);

private:
	void refresh_hosts(const hosts_info_list_t& hosts,
					   hosts_info_list_t& oustanding_hosts,
					   hosts_info_list_t& new_hosts);

	void refresh_handles(const handles_info_list_t& handles,
						 handles_info_list_t& oustanding_handles,
						 handles_info_list_t& new_handles);

	void remove_outstanding_handles(const handles_info_list_t& handles);
	void create_new_handles(const handles_info_list_t& handles, const hosts_info_list_t& hosts);

	void log_refreshed_hosts_and_handles(const hosts_info_list_t& hosts,
										 const handles_info_list_t& handles);

	void enqueue_responce_callback(cached_response_prt_t response);
	void dispatch_responces();

	// send collected statistics to global stats collector
	void update_statistics();

	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config();
	boost::shared_ptr<cocaine::dealer::context> context();

private:
	// service information
	service_info<LSD_T> info_;

	// hosts map (ip, hostname)
	hosts_map_t hosts_;

	// handles map (handle name, handle ptr)
	handles_map_t handles_;

	// service messages for non-existing handles <handle name, handle ptr>
	unhandled_messages_map_t unhandled_messages_;

	// responces map <handle name, responces queue ptr>
	responces_map_t received_responces_;

	// dealer context
	boost::shared_ptr<cocaine::dealer::context> context_;

	// total cache size
	size_t cache_size_;

	// statistics
	service_stats stats_;

	boost::thread thread_;
	boost::mutex mutex_;
	volatile bool is_running_;

	// responses callbacks
	registered_callbacks_map_t responses_callbacks_map_;
};

template <typename LSD_T>
service<LSD_T>::service(const service_info<LSD_T>& info, boost::shared_ptr<cocaine::dealer::context> context) :
	info_(info),
	context_(context),
	cache_size_(0),
	is_running_(false)
{
	update_statistics();

	// run response dispatch thread
	is_running_ = true;
	thread_ = boost::thread(&service<LSD_T>::dispatch_responces, this);
}

template <typename LSD_T>
service<LSD_T>::~service() {
	is_running_ = false;
	thread_.join();
}

template <typename LSD_T> void
service<LSD_T>::dispatch_responces() {
	while (is_running_) {
		boost::mutex::scoped_lock lock(mutex_);

		// go through all callbacks
		registered_callbacks_map_t::iterator it = responses_callbacks_map_.begin();
		for (; it != responses_callbacks_map_.end(); ++it) {

			// get responces queue for registered callback
			responces_map_t::iterator qit = received_responces_.find(it->first);
			if (qit != received_responces_.end()) {

				// get first responce from queue
				responces_deque_ptr_t handle_resp_queue = qit->second;

				if (!handle_resp_queue->empty()) {
					cached_response_prt_t resp_ptr = handle_resp_queue->front();
					registered_callback_t callback = it->second;

					// create simplified response
					response resp;
					resp.uuid = resp_ptr->uuid();
					resp.data = resp_ptr->data().data();
					resp.size = resp_ptr->data().size();

					response_info resp_info;
					resp_info.error = resp_ptr->error_code();
					resp_info.error_msg = resp_ptr->error_message();
					resp_info.service = resp_ptr->path().service_name;
					resp_info.handle = resp_ptr->path().handle_name;

					// invoke callback for given handle and response
					callback(resp, resp_info);

					// remove processed response
					handle_resp_queue->pop_front();
				}
			}
		}

	}
}

template <typename LSD_T> boost::shared_ptr<cocaine::dealer::context>
service<LSD_T>::context() {
	if (!context_.get()) {
		throw error("dealer context object is empty at " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return context_;
}

template <typename LSD_T> boost::shared_ptr<base_logger>
service<LSD_T>::logger() {
	return context()->logger();
}

template <typename LSD_T> boost::shared_ptr<configuration>
service<LSD_T>::config() {
	boost::shared_ptr<configuration> conf = context()->config();
	if (!conf.get()) {
		throw error("configuration object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return conf;
}

template <typename LSD_T> void
service<LSD_T>::log_refreshed_hosts_and_handles(const hosts_info_list_t& hosts,
												const handles_info_list_t& handles)
{
	logger()->log(PLOG_DEBUG, "service %s refreshed with:", info_.name_.c_str());

	for (size_t i = 0; i < hosts.size(); ++i) {
		std::stringstream tmp;
		tmp << "host - " << hosts[i];
		logger()->log(PLOG_DEBUG, tmp.str());
	}

	for (size_t i = 0; i < handles.size(); ++i) {
		std::stringstream tmp;
		tmp << "handle - " << handles[i];
		logger()->log(PLOG_DEBUG, tmp.str());
	}
}

template <typename LSD_T> bool
service<LSD_T>::register_responder_callback(boost::function<void(const response&, const response_info&)> callback,
											const std::string& handle_name)
{
	boost::mutex::scoped_lock lock(mutex_);

	registered_callbacks_map_t::iterator it = responses_callbacks_map_.find(handle_name);

	// check whether such callback is already registered
	if (it != responses_callbacks_map_.end()) {
		return false;
	}

	responses_callbacks_map_[handle_name] = callback;
	return true;
}

template <typename LSD_T> void
service<LSD_T>::enqueue_responce_callback(cached_response_prt_t response) {

	// validate response
	if (!response) {
		std::string error_str = "received empty response object!";
		error_str += " service: " + info_.name_;
		error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_str);
	}

	boost::mutex::scoped_lock lock(mutex_);
	const message_path& path = response->path();

	// see whether there exists registered callback for response handle
	registered_callbacks_map_t::iterator callback_it = responses_callbacks_map_.find(path.handle_name);

	// check whether such callback is already registered
	if (callback_it == responses_callbacks_map_.end()) {

		// no callback -- drop response
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
			throw error(error_str);
		}
	}

	// add responce to queue
	handle_resp_queue->push_back(response);
}

template <typename LSD_T> void
service<LSD_T>::update_statistics() {

	// reset containers
	stats_.unhandled_messages.clear();
	stats_.handles.clear();

	// gather statistics of unhandled messages
	unhandled_messages_map_t::iterator it = unhandled_messages_.begin();
	for (; it != unhandled_messages_.end(); ++it) {
		if (it->second) {
			stats_.unhandled_messages[it->first] = it->second->size();
		}
		else {
			std::string error_str = "found empty unhandled messages queue object!";
			error_str += " service: " + info_.name_ + " handle: " + it->first;
			error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw error(error_str);
		}
	}

	// gather hosts info
	stats_.hosts = hosts_;

	// gather handles info
	typename handles_map_t::iterator it2 = handles_.begin();

	for (; it2 != handles_.end(); ++it2) {
		if (it2->second) {
			stats_.handles.push_back(it2->first);
		}
		else {
			std::string error_str = "found empty handle object!";
			error_str += " service: " + info_.name_ + " handle: " + it2->first;
			error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw error(error_str);
		}
	}

	// post collected statistics to collector obj
	context()->stats()->update_service_stats(info_.name_, stats_);
}

template <typename LSD_T> void
service<LSD_T>::refresh_hosts_and_handles(const hosts_info_list_t& hosts,
										  const handles_info_list_t& handles)
{
	boost::mutex::scoped_lock lock(mutex_);

	//log_refreshed_hosts_and_handles(hosts, handles);
	//return;

	// refresh hosts
	hosts_info_list_t outstanding_hosts;
	hosts_info_list_t new_hosts;
	refresh_hosts(hosts, outstanding_hosts, new_hosts);

	// refresh handles
	handles_info_list_t outstanding_handles;
	handles_info_list_t new_handles;
	refresh_handles(handles, outstanding_handles, new_handles);

	// remove oustanding handles
	remove_outstanding_handles(outstanding_handles);

	// make list of hosts
	hosts_info_list_t hosts_v;
	for (typename hosts_map_t::iterator it = hosts_.begin(); it != hosts_.end(); ++it) {
		hosts_v.push_back(host_info<LSD_T>(it->first, it->second));
	}

	// reconnect existing handles if we have outstanding hosts
	if (!outstanding_hosts.empty()) {
		typename handles_map_t::iterator it = handles_.begin();
		for (;it != handles_.end(); ++it) {
			it->second->reconnect(hosts_v);
		}
	}
	else {
		// add connections to new hosts
		if (!new_hosts.empty()) {
			typename handles_map_t::iterator it = handles_.begin();
			for (;it != handles_.end(); ++it) {
				it->second->connect_new_hosts(new_hosts);
			}
		}
	}

	lock.unlock();

	// create new handles if any
	create_new_handles(new_handles, hosts_v);

	lock.lock();
	update_statistics();
}

template <typename LSD_T> void
service<LSD_T>::refresh_hosts(const hosts_info_list_t& hosts,
							  hosts_info_list_t& oustanding_hosts,
							  hosts_info_list_t& new_hosts)
{
	// check for outstanding hosts
	for (typename hosts_map_t::iterator it = hosts_.begin(); it != hosts_.end(); ++it) {
		bool found = false;
		for (size_t i = 0; i < hosts.size(); ++i) {
			if (hosts[i].ip_ == it->first) {
				found = true;
			}
		}

		if (!found) {
			oustanding_hosts.push_back(host_info<LSD_T>(it->first, it->second));
		}
	}

	// check for new hosts
	for (size_t i = 0; i < hosts.size(); ++i) {
		typename hosts_map_t::iterator it = hosts_.find(hosts[i].ip_);
		if (it == hosts_.end()) {
			new_hosts.push_back(hosts[i]);
		}
	}

	hosts_.clear();

	// replace hosts list with new hosts
	for (size_t i = 0; i < hosts.size(); ++i) {
		hosts_[hosts[i].ip_] = hosts[i].hostname_;
	}
}

template <typename LSD_T> void
service<LSD_T>::refresh_handles(const handles_info_list_t& handles,
					 	 	    handles_info_list_t& oustanding_handles,
					 	 	    handles_info_list_t& new_handles)
{
	// check for outstanding hosts
	for (typename handles_map_t::iterator it = handles_.begin(); it != handles_.end(); ++it) {
		bool found = false;
		for (size_t i = 0; i < handles.size(); ++i) {
			if (it->second->info() == handles[i]) {
				found = true;
			}
		}

		if (!found) {
			oustanding_handles.push_back(it->second->info());
		}
	}

	// check for new handles
	for (size_t i = 0; i < handles.size(); ++i) {
		typename handles_map_t::iterator it = handles_.find(handles[i].name_);
		if (it == handles_.end()) {
			new_handles.push_back(handles[i]);
		}
	}
}

template <typename LSD_T> void
service<LSD_T>::remove_outstanding_handles(const handles_info_list_t& handles) {
	// no handles to destroy
	if (handles.empty()) {
		return;
	}

	logger()->log("remove_outstanding_handles");

	// destroy handles
	for (size_t i = 0; i < handles.size(); ++i) {
		typename handles_map_t::iterator it = handles_.find(handles[i].name_);

		if (it != handles_.end()) {
			handle_ptr_t handle = it->second;

			// check handle
			if (!handle) {
				std::string error_str = "service handle object is empty. service: " + info_.name_;
				error_str += ", handle: " + handles[i].name_;
				error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
				throw error(error_str);
			}

			// immediately terminate all handle activity
			handle->disconnect();
			boost::shared_ptr<message_cache> msg_cache = handle->messages_cache();

			// check handle message cache
			if (!msg_cache) {
				std::string error_str = "handle message cache object is empty. service: " + info_.name_;
				error_str += ", handle: " + handles[i].name_;
				error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
				throw error(error_str);
			}

			// consolidate all handle messages
			msg_cache->make_all_messages_new();

			// find corresponding unhandled msgs queue
			unhandled_messages_map_t::iterator it = unhandled_messages_.find(handle->info().name_);

			// should not find a queue with messages!
			if (it != unhandled_messages_.end()) {
				messages_deque_ptr_t msg_queue = it->second;

				if (msg_queue && !msg_queue->empty()) {
					std::string error_str = "found unhandled non-empty message queue with existing handle!";
					error_str += " service: " + info_.name_ + ", handle: " + handles[i].name_;
					error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
					throw error(error_str);
				}

				// remove empty queue if any
				unhandled_messages_.erase(it);
			}

			// move handle messages to unhandled messages map in service
			messages_deque_ptr_t handle_msg_queue = msg_cache->new_messages();

			// validate handle queue
			if (!handle_msg_queue) {
				std::string error_str = "found empty handle message queue when handle exists!";
				error_str += " service: " + info_.name_ + ", handle: " + handles[i].name_;
				error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
				throw error(error_str);
			}

			// in case there are messages, store them
			if (!handle_msg_queue->empty()) {
				unhandled_messages_[handle->info().name_] = handle_msg_queue;
			}
		}

		handles_.erase(it);
	}

	update_statistics();
}

template <typename LSD_T> void
service<LSD_T>::create_new_handles(const handles_info_list_t& handles, const hosts_info_list_t& hosts) {
	// no handles to create
	if (handles.empty()) {
		return;
	}

	boost::mutex::scoped_lock lock(mutex_);

	// create handles
	for (size_t i = 0; i < handles.size(); ++i) {
		handle_ptr_t handle_ptr;
		handle_info<LSD_T> handle_info = handles[i];
		handle_info.service_name_ = info_.name_;
		handle_ptr.reset(new handle<LSD_T>(handle_info, context_, hosts));

		// set responce callback
		typedef typename handle<LSD_T>::responce_callback_t resp_callback;
		resp_callback callback = boost::bind(&service<LSD_T>::enqueue_responce_callback, this, _1);
		handle_ptr->set_responce_callback(callback);

		// find corresponding unhandled msgs queue
		unhandled_messages_map_t::iterator it = unhandled_messages_.find(handles[i].name_);

		// validate queue
		if (it != unhandled_messages_.end()) {
			messages_deque_ptr_t msg_queue = it->second;

			// add existing message queue to handle
			if (msg_queue.get() && !msg_queue->empty()) {

				// validate handle's message cache object
				if (handle_ptr->messages_cache().get()) {
					logger()->log(PLOG_DEBUG, "appending existing mesage queue for handle %s, queue size: %d", handles[i].name_.c_str(), msg_queue->size());
					handle_ptr->messages_cache()->append_message_queue(msg_queue);
				}
				else {
					std::string error_str = "found empty handle message queue when handle exists!";
					error_str += " service: " + info_.name_ + ", handle: " + handles[i].name_;
					error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
					throw error(error_str);
				}
			}

			// remove message queue from unhandled messages map
			unhandled_messages_.erase(it);
		}

		// add handle to storage and connect it
		handles_[handles[i].name_] = handle_ptr;

		lock.unlock();
		handles_[handles[i].name_]->connect(hosts);
		lock.lock();
	}

	update_statistics();
}

template <typename LSD_T> void
service<LSD_T>::send_message(cached_message_prt_t message) {
	boost::mutex::scoped_lock lock(mutex_);

	if (!message) {
		std::string error_str = "message object is empty. service: " + info_.name_;
		error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
		throw error(error_str);
	}

	const std::string& handle_name = message->path().handle_name;

	// find existing handle to enqueue message
	typename handles_map_t::iterator it = handles_.find(handle_name);
	if (it != handles_.end()) {
		handle_ptr_t handle_ptr = it->second;

		// make sure we have valid handle
		if (handle_ptr) {
			handle_ptr->enqueue_message(message);
			cache_size_ += message->container_size();
		}
		else {
			std::string error_str = "handle object " + handle_name;
			error_str += " for service: " + info_.name_ + " is empty.";
			error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw error(error_str);
		}
	}
	else {
		// if no handle, store locally
		unhandled_messages_map_t::iterator it = unhandled_messages_.find(handle_name);

		// check for existing messages queue for handle
		messages_deque_ptr_t queue_ptr;

		if (it == unhandled_messages_.end()) {
			queue_ptr.reset(new cached_messages_deque_t);
			queue_ptr->push_back(message);
			unhandled_messages_[handle_name] = queue_ptr;
		}
		else {
			queue_ptr = it->second;

			// validate msg queue
			if (!queue_ptr) {
				std::string error_str = "found empty message queue object in unhandled messages map!";
				error_str += " service: " + info_.name_ + ", handle: " + handle_name;
				error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
				throw error(error_str);
			}

			queue_ptr->push_back(message);
		}

		cache_size_ += message->container_size();
	}

	update_statistics();
}

template <typename LSD_T> size_t
service<LSD_T>::cache_size() const {
	return cache_size_;
}

template<typename T>
std::ostream& operator << (std::ostream& out, const service<T>& s) {
	out << "----- service info: -----\n";
	out << s.info_;

	return out;
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_SERVICE_HPP_INCLUDED_
