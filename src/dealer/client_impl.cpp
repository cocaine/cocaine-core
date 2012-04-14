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

#include <stdexcept>

#include <boost/current_function.hpp>

#include "cocaine/dealer/core/cached_message.hpp"
#include "cocaine/dealer/core/request_metadata.hpp"
#include "cocaine/dealer/utils/data_container.hpp"
#include "cocaine/dealer/utils/persistent_data_container.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/heartbeats/heartbeats_collector.hpp"
#include "cocaine/dealer/heartbeats/http_hosts_fetcher.hpp"
#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"

#include "cocaine/dealer/core/client_impl.hpp"

namespace cocaine {
namespace dealer {

typedef cached_message<data_container, request_metadata> message_t;
typedef cached_message<persistent_data_container, persistent_request_metadata> p_message_t;

client_impl::client_impl(const std::string& config_path) :
	messages_cache_size_(0)
{
	// create dealer context
	std::string ctx_error_msg = "could not create dealer context at: " + std::string(BOOST_CURRENT_FUNCTION) + " ";

	try {
		context_.reset(new cocaine::dealer::context(config_path));
	}
	catch (const std::exception& ex) {
		throw internal_error(ctx_error_msg + ex.what());
	}

	logger()->log("creating client.");

	// get services list
	const configuration::services_list_t& services_info_list = config()->services_list();

	// create services
	configuration::services_list_t::const_iterator it = services_info_list.begin();
	for (; it != services_info_list.end(); ++it) {
		boost::shared_ptr<service_t> service_ptr(new service_t(it->second, context_));

		logger()->log("STARTING SERVICE [%s]", it->second.name_.c_str());

		if (config()->message_cache_type() == PERSISTENT) {
			load_cached_messages_for_service(service_ptr);
		}

		services_[it->first] = service_ptr;
	}

	connect();
	logger()->log("client created.");
}

client_impl::~client_impl() {
	disconnect();
	logger()->log("client destroyed.");
}

void
client_impl::connect() {
	boost::shared_ptr<configuration> conf;

	if (!context_) {
		std::string error_msg = "dealer context is NULL at: " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}
	else {
		conf = context_->config();
	}

	if (!conf) {
		std::string error_msg = "configuration object is empty at: " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}

	if (conf->autodiscovery_type() == AT_FILE) {
		logger()->log("creating file heartbeats collector");
		heartbeats_collector_.reset(new heartbeats_collector<file_hosts_fetcher>(conf, context()->zmq_context()));
	}
	else if (conf->autodiscovery_type() == AT_HTTP) {
		logger()->log("creating http heartbeats collector");
		heartbeats_collector_.reset(new heartbeats_collector<http_hosts_fetcher>(conf, context()->zmq_context()));
	}

	heartbeats_collector_->set_callback(boost::bind(&client_impl::service_hosts_pinged_callback, this, _1, _2, _3));
	//heartbeats_collector_->set_logger(logger());
	heartbeats_collector_->run();
}

void
client_impl::disconnect() {
	// stop collecting heartbeats
	if (heartbeats_collector_.get()) {
		heartbeats_collector_->stop();
	}
	else {
		std::string error_msg = "empty heartbeats collector object at ";
		error_msg += std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}

	// stop services
	//services_map_t::iterator it = services_.begin();
	//for (; it != services_.end(); ++it) {
		//if(it->second)
	//}
}

void
client_impl::service_hosts_pinged_callback(const service_info_t& s_info,
										   const std::vector<host_info_t>& hosts,
										   const std::vector<handle_info_t>& handles)
{
	boost::mutex::scoped_lock lock(mutex_);

	// find corresponding service
	services_map_t::iterator it = services_.find(s_info.name_);

	// populate service with pinged hosts and handles
	if (it != services_.end()) {
		if (it->second.get()) {
			it->second->refresh_hosts_and_handles(hosts, handles);
		}
		else {
			std::string error_msg = "empty service object with dealer name " + s_info.name_;
			error_msg += " was found in services. at: " + std::string(BOOST_CURRENT_FUNCTION);
			throw internal_error(error_msg);
		}
	}
	else {
		std::string error_msg = "dealer service with name " + s_info.name_;
		error_msg += " was not found in services. at: " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_msg);
	}
}

boost::shared_ptr<message_iface>
client_impl::create_message(const void* data,
							size_t size,
							const message_path& path,
							const message_policy& policy)
{
	boost::mutex::scoped_lock lock(mutex_);
	boost::shared_ptr<message_iface> msg;

	if (config()->message_cache_type() == RAM_ONLY) {
		msg.reset(new message_t(path, policy, data, size));
		logger()->log(PLOG_DEBUG, "created message, size: %d bytes, uuid: %s", size, msg->uuid().c_str());
	}
	else if (config()->message_cache_type() == PERSISTENT) {
		p_message_t* msg_ptr = new p_message_t(path, policy, data, size);
		logger()->log(PLOG_DEBUG, "created message, size: %d bytes, uuid: %s", size, msg_ptr->uuid().c_str());

		eblob eb = context()->storage()->get_eblob(path.service_name);
		
		// init metadata and write to storage
		msg_ptr->mdata_container().set_eblob(eb);
		msg_ptr->mdata_container().commit_data();
		msg_ptr->mdata_container().data_size = size;

		// init data and write to storage
		msg_ptr->data_container().set_eblob(eb, msg_ptr->uuid());
		msg_ptr->data_container().commit_data();

		logger()->log(PLOG_DEBUG, "commited message with uuid: " + msg_ptr->uuid() + " to persistent storage.");

		msg.reset(msg_ptr);
	}

	return msg;
}

std::string
client_impl::send_message(const boost::shared_ptr<message_iface>& msg, response_callback callback) {
	boost::mutex::scoped_lock lock(mutex_);

	// find service to send message to
	std::string uuid;
	services_map_t::iterator it = services_.find(msg->path().service_name);

	if (it == services_.end()) {
		std::string error_str = "no service with name " + msg->path().service_name;
		error_str += " found at " + std::string(BOOST_CURRENT_FUNCTION);
		throw dealer_error(location_error, error_str);
	}

	uuid = msg->uuid();

	// assign callback
	std::string message_str = "registering callback for message with uuid: " + msg->uuid();
	logger()->log(PLOG_DEBUG, message_str);

	lock.unlock();
	it->second->register_responder_callback(uuid, callback);
	lock.lock();

	message_str = "registered callback for message with uuid: " + msg->uuid();
	logger()->log(PLOG_DEBUG, message_str);

	// send message to service
	if (it->second) {
		lock.unlock();
		it->second->send_message(msg);
		lock.lock();
	}
	else {
		std::string error_str = "object for service with name " + msg->path().service_name;
		error_str += " is emty at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}

	message_str = "enqueued message with uuid: %s to [%s.%s]";
	logger()->log(PLOG_DEBUG, message_str.c_str(), uuid.c_str(),
				  msg->path().service_name.c_str(), msg->path().handle_name.c_str());

	return uuid;
}

void
client_impl::unset_response_callback(const std::string& message_uuid,
								 	 const message_path& path)
{
	boost::mutex::scoped_lock lock(mutex_);

	logger()->log(PLOG_DEBUG, "TMP - unset_response_callback in client_impl");

	// check for services
	services_map_t::iterator it = services_.find(path.service_name);
	if (it == services_.end()) {
		std::string error_msg = "message sent to unknown service \"" + path.service_name + "\"";
		error_msg += " at: " + std::string(BOOST_CURRENT_FUNCTION);
		error_msg += " please make sure you've defined service in dealer configuration file.";
		throw dealer_error(location_error, error_msg);
	}

	if (!it->second) {
		throw internal_error("service object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	lock.unlock();

	// assign to service
	it->second->unregister_responder_callback(message_uuid);

	//std::string message_str = "unregistered callback for message with uuid: " + message_uuid;
	//logger()->log(PLOG_DEBUG, message_str);
}

boost::shared_ptr<context>
client_impl::context() {
	if (!context_) {
		throw internal_error("dealer context object is empty at " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return context_;
}

boost::shared_ptr<base_logger>
client_impl::logger() {
	return context()->logger();
}

boost::shared_ptr<configuration>
client_impl::config() {
	boost::shared_ptr<configuration> conf = context()->config();
	if (!conf.get()) {
		throw internal_error("configuration object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return conf;
}

void
client_impl::load_cached_messages_for_service(boost::shared_ptr<service_t>& service) {	
	// validate input
	std::string service_name = service->info().name_;

	if (!service) {
		std::string error_str = "object for service with name " + service_name;
		error_str += " is emty at " + std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}

	// show statistics
	eblob blob = this->context()->storage()->get_eblob(service_name);
	std::string log_str = "SERVICE [%s] is restoring %d messages from persistent cache...";
	logger()->log(PLOG_DEBUG, log_str.c_str(), service_name.c_str(), (int)(blob.items_count() / 2));

	restored_service_tmp_ptr_ = service;

	// restore messages from
	if (blob.items_count() > 0) {
		eblob::iteration_callback_t callback;
		callback = boost::bind(&client_impl::storage_iteration_callback, this, _1, _2, _3);
		blob.iterate(callback, 0, 0);
	}

	restored_service_tmp_ptr_.reset();
}

void
client_impl::storage_iteration_callback(void* data, uint64_t size, int column) {
	if (!restored_service_tmp_ptr_) {
		throw internal_error("service object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	if (column == 0 && (!data || size == 0)) {
		throw internal_error("metadata is missing at: " + std::string(BOOST_CURRENT_FUNCTION));	
	}

	// get service eblob
	std::string service_name = restored_service_tmp_ptr_->info().name_;
	eblob eb = context()->storage()->get_eblob(service_name);

	p_message_t* msg_ptr = new p_message_t();
	msg_ptr->mdata_container().load_data(data, size);
	msg_ptr->mdata_container().set_eblob(eb);
	msg_ptr->data_container().init_from_message_cache(eb, msg_ptr->mdata_container().uuid, size);

	// send message to service
	boost::shared_ptr<message_iface> msg(msg_ptr);
	restored_service_tmp_ptr_->send_message(msg);
}

} // namespace dealer
} // namespace cocaine
