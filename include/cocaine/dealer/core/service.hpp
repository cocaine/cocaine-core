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
#include <deque>

#include <zmq.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/thread/thread.hpp>
#include <boost/function.hpp>

#include "cocaine/dealer/response.hpp"

#include "cocaine/dealer/core/handle.hpp"
#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/core/host_info.hpp"
#include "cocaine/dealer/core/handle_info.hpp"
#include "cocaine/dealer/core/service_info.hpp"
#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/core/cached_response.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"
#include "cocaine/dealer/utils/refresher.hpp"
#include "cocaine/dealer/storage/eblob.hpp"

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
	typedef std::map<std::string, boost::weak_ptr<response> > registered_callbacks_map_t;

public:
	service(const service_info<LSD_T>& info, boost::shared_ptr<cocaine::dealer::context> context);
	virtual ~service();

	void refresh_hosts_and_handles(const hosts_info_list_t& hosts,
								   const std::vector<handle_info<LSD_T> >& handles);

	void send_message(cached_message_prt_t message);

	service_info<LSD_T> info() const;

	void register_responder_callback(const std::string& message_uuid, const boost::shared_ptr<response>& response);
	void unregister_responder_callback(const std::string& message_uuid);

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

	void enqueue_responce(cached_response_prt_t response);
	void dispatch_responces();
	bool responces_queues_empty() const;

	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config();
	boost::shared_ptr<cocaine::dealer::context> context();

	void check_for_deadlined_messages();

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

	static const int deadline_check_interval = 1;
};

template <typename LSD_T>
service<LSD_T>::service(const service_info<LSD_T>& info, boost::shared_ptr<cocaine::dealer::context> context) :
	info_(info),
	context_(context),
	is_running_(false)
{
	// run response dispatch thread
	is_running_ = true;
	thread_ = boost::thread(&service<LSD_T>::dispatch_responces, this);

	// run timed out messages checker
	//deadlined_messages_refresher_.reset(new refresher(boost::bind(&service<LSD_T>::check_for_deadlined_messages, this), deadline_check_interval));
}

template <typename LSD_T>
service<LSD_T>::~service() {
	is_running_ = false;
	cond_.notify_one();
	thread_.join();
}

template <typename LSD_T> service_info<LSD_T>
service<LSD_T>::info() const {
	return info_;
}

template <typename LSD_T> bool
service<LSD_T>::responces_queues_empty() const {
	responces_map_t::const_iterator it = received_responces_.begin();
	for (; it != received_responces_.end(); ++it) {
		responces_deque_ptr_t handle_resp_queue = it->second;
		if (!handle_resp_queue->empty()) {
			return false;
		}
	}

	return true;
}

template <typename LSD_T> void
service<LSD_T>::dispatch_responces() {
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
						lock.unlock();
						boost::weak_ptr<response> response_wptr = it->second;
						boost::shared_ptr<response> response_ptr = response_wptr.lock();
						
						if (!response_ptr) {
							//std::cout << "callback expired\n";
							responses_callbacks_map_.erase(it);
						}
						else {
							//std::cout << "calling callback\n";
							response_ptr->response_callback(resp_data, resp_info);
						}
					}
				}
				catch (...) {
				}

				// remove processed response
				handle_resp_queue->pop_front();
			}
		}
	}
}

template <typename LSD_T> boost::shared_ptr<cocaine::dealer::context>
service<LSD_T>::context() {
	if (!context_.get()) {
		throw internal_error("dealer context object is empty at " + std::string(BOOST_CURRENT_FUNCTION));
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
		throw internal_error("configuration object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return conf;
}

template <typename LSD_T> void
service<LSD_T>::log_refreshed_hosts_and_handles(const hosts_info_list_t& hosts,
												const handles_info_list_t& handles)
{
	logger()->log(PLOG_INFO, "service %s refreshed with: ", info_.name_.c_str());

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

template <typename LSD_T> void
service<LSD_T>::register_responder_callback(const std::string& message_uuid, const boost::shared_ptr<response>& resp)
{
	boost::mutex::scoped_lock lock(mutex_);
	boost::weak_ptr<response> wptr(resp);
	responses_callbacks_map_[message_uuid] = wptr;
}

template <typename LSD_T> void
service<LSD_T>::unregister_responder_callback(const std::string& message_uuid) {
	boost::mutex::scoped_lock lock(mutex_);
	registered_callbacks_map_t::iterator callback_it = responses_callbacks_map_.find(message_uuid);

	// is there a callback for given response uuid?
	if (callback_it == responses_callbacks_map_.end()) {
		return;
	}

	responses_callbacks_map_.erase(callback_it);
}

template <typename LSD_T> void
service<LSD_T>::enqueue_responce(cached_response_prt_t response) {
	boost::mutex::scoped_lock lock(mutex_);

	// validate response
	if (!response) {
		std::string error_str = "received empty response object!";
		error_str += " service: " + info_.name_;
		error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}

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
	lock.unlock();
	remove_outstanding_handles(outstanding_handles);
	lock.lock();

	// make list of hosts
	hosts_info_list_t hosts_v;
	for (typename hosts_map_t::iterator it = hosts_.begin(); it != hosts_.end(); ++it) {
		hosts_v.push_back(host_info<LSD_T>(it->first, it->second));
	}

	// reconnect existing handles if we have outstanding hosts
	if (!outstanding_hosts.empty()) {
		typename handles_map_t::iterator it = handles_.begin();
		for (;it != handles_.end(); ++it) {
			lock.unlock();
			it->second->reconnect(hosts_v);
			lock.lock();
		}
	}
	else {
		// add connections to new hosts
		if (!new_hosts.empty()) {
			typename handles_map_t::iterator it = handles_.begin();
			for (;it != handles_.end(); ++it) {
				lock.unlock();
				it->second->connect_new_hosts(new_hosts);
				lock.lock();
			}
		}
	}

	// create new handles if any
	lock.unlock();
	create_new_handles(new_handles, hosts_v);
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
		typename handles_map_t::iterator it = handles_.find(handles[i].name_);

		if (it != handles_.end()) {
			handle_ptr_t handle = it->second;

			// check handle
			if (!handle) {
				std::string error_str = "service handle object is empty. service: " + info_.name_;
				error_str += ", handle: " + handles[i].name_;
				error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
				throw internal_error(error_str);
			}

			// immediately terminate all handle activity
			lock.unlock();
			handle->disconnect();
			lock.lock();

			boost::shared_ptr<message_cache> msg_cache = handle->messages_cache();

			// check handle message cache
			if (!msg_cache) {
				std::string error_str = "handle message cache object is empty. service: " + info_.name_;
				error_str += ", handle: " + handles[i].name_;
				error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
				throw internal_error(error_str);
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
					throw internal_error(error_str);
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
				throw internal_error(error_str);
			}

			// in case there are messages, store them
			if (!handle_msg_queue->empty()) {
				unhandled_messages_[handle->info().name_] = handle_msg_queue;
			}
		}

		handles_.erase(it);
	}
}

template <typename LSD_T> void
service<LSD_T>::create_new_handles(const handles_info_list_t& handles, const hosts_info_list_t& hosts) {
	boost::mutex::scoped_lock lock(mutex_);

	// no handles to create
	if (handles.empty()) {
		return;
	}

	// create handles
	for (size_t i = 0; i < handles.size(); ++i) {
		handle_ptr_t handle_ptr;
		handle_info<LSD_T> handle_info = handles[i];
		handle_info.service_name_ = info_.name_;

		lock.unlock();
		handle_ptr.reset(new handle<LSD_T>(handle_info, context_, hosts));
		lock.lock();

		// set responce callback
		typedef typename handle<LSD_T>::responce_callback_t resp_callback;
		resp_callback callback = boost::bind(&service<LSD_T>::enqueue_responce, this, _1);

		lock.unlock();
		handle_ptr->set_responce_callback(callback);
		lock.lock();

		// find corresponding unhandled msgs queue
		unhandled_messages_map_t::iterator it = unhandled_messages_.find(handles[i].name_);

		// validate queue
		if (it != unhandled_messages_.end()) {
			messages_deque_ptr_t msg_queue = it->second;

			// add existing message queue to handle
			if (msg_queue.get() && !msg_queue->empty()) {

				// validate handle's message cache object
				lock.unlock();
				if (handle_ptr->messages_cache().get()) {
					logger()->log(PLOG_DEBUG, "appending existing mesage queue for [%s.%s], queue size: %d",
								  info_.name_.c_str(), handles[i].name_.c_str(), msg_queue->size());
					handle_ptr->messages_cache()->append_message_queue(msg_queue);
					handle_ptr->notify_new_messages_enqueued();
				}
				else {
					std::string error_str = "found empty handle message queue when handle exists!";
					error_str += " service: " + info_.name_ + ", handle: " + handles[i].name_;
					error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
					throw internal_error(error_str);
				}
				lock.lock();
			}

			// remove message queue from unhandled messages map
			unhandled_messages_.erase(it);
		}

		// add handle to storage and connect it
		handles_[handles[i].name_] = handle_ptr;
		//handles_[handles[i].name_]->connect(hosts);
	}
}

template <typename LSD_T> void
service<LSD_T>::send_message(cached_message_prt_t message) {
	boost::mutex::scoped_lock lock(mutex_);

	if (!message) {
		std::string error_str = "message object is empty. service: " + info_.name_;
		error_str += ". at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}

	const std::string& handle_name = message->path().handle_name;

	// find existing handle to enqueue message
	typename handles_map_t::iterator it = handles_.find(handle_name);
	if (it != handles_.end()) {
		handle_ptr_t handle_ptr = it->second;

		// make sure we have valid handle
		if (handle_ptr) {
			lock.unlock();
			handle_ptr->enqueue_message(message);
			lock.lock();
		}
		else {
			std::string error_str = "handle object " + handle_name;
			error_str += " for service: " + info_.name_ + " is empty.";
			error_str += " at " + std::string(BOOST_CURRENT_FUNCTION);
			throw internal_error(error_str);
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
				throw internal_error(error_str);
			}

			queue_ptr->push_back(message);
		}
	}
}

template<typename LSD_T> void
service<LSD_T>::check_for_deadlined_messages() {
	//unhandled_messages_
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
