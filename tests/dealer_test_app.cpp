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

#include <iostream>

#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include "cocaine/dealer/dealer.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"
#include <cocaine/dealer/utils/error.hpp>

using namespace cocaine::dealer;
using namespace boost::program_options;

int sent_messages = 0;

void worker(dealer_t* d,
			std::vector<int>* dealer_messages_count,
			int dealer_index)
{
	message_path_t path("rimz_app", "my-super-handle");
	message_policy_t policy;
	policy.deadline = 0.0;
	policy.max_retries = -1;
	std::string payload = "response chunk: ";

	while ((*dealer_messages_count)[dealer_index] >= 0) {
		try {
			boost::shared_ptr<response_t> resp;

			if (d) {
				resp = d->send_message(payload.data(), payload.size(), path, policy);
			}

			data_container data;
			while (resp->get(&data)) {
				//std::cout << std::string(reinterpret_cast<const char*>(data.data()), 0, data.size()) << std::endl;
			}
		}
		catch (const dealer_error& err) {
			std::cout << "error code: " << err.code() << ", error message: " << err.what() << std::endl;
		}
		catch (const std::exception& ex) {
			std::cout << "error message: " << ex.what() << std::endl;
		}
		catch (...) {
			std::cout << "caught exception, no error message." << std::endl;
		}

		(*dealer_messages_count)[dealer_index] = (*dealer_messages_count)[dealer_index] - 1;
		sent_messages++;
	}
}

void create_client(size_t dealers_count, size_t threads_per_dealer, size_t messages_count) {
	std::string config_path = "tests/config_example.json";

	typedef boost::ptr_vector<boost::thread> thread_pool;
	typedef boost::ptr_vector<thread_pool> thread_pools_list;

	std::cout << "----------------------------------- test info -------------------------------------------\n";
	std::cout << "sending " << dealers_count * messages_count << " messages using ";
	std::cout << dealers_count << " dealers with " << threads_per_dealer << " threads each.\n";
	std::cout << "-----------------------------------------------------------------------------------------\n";
	
	progress_timer timer;

	std::vector<int> dealer_messages_count;
	boost::ptr_vector<dealer_t> dealers;

	for (size_t i = 0; i < dealers_count; ++i) {
		dealers.push_back(new dealer_t(config_path));
		dealer_messages_count.push_back(messages_count);
	}

	// create threads
	std::cout << "sending messages...\n";

	thread_pools_list pools;
	for (size_t i = 0; i < dealers_count; ++i) {
		thread_pool* pool = new thread_pool;

		for (size_t j = 0; j < threads_per_dealer; ++j) {
			boost::thread* th;
			th = new boost::thread(&worker,
								   &(dealers[i]),
								   &dealer_messages_count,
								   i);
			pool->push_back(th);
		}

		pools.push_back(pool);
	}

	for (size_t i = 0; i < dealers_count; ++i) {
		for (size_t j = 0; j < threads_per_dealer; ++j) {
			pools[i][j].join();
		}
	}

	std::cout << "sending messages done.\n";

	std::cout << "----------------------------------- test results ----------------------------------------\n";
	std::cout << "elapsed: " << timer.elapsed().as_double() << std::endl;
	std::cout << "sent: " << sent_messages << " messages.\n";
	std::cout << "approx performance: " << sent_messages / timer.elapsed().as_double() << " rps." << std::endl;
	
	std::cout << "----------------------------------- shutting dealers down -------------------------------\n";
}

int
main(int argc, char** argv) {
	try {
		options_description desc("Allowed options");
		desc.add_options()
			("help", "Produce help message")
			("dealers,d", value<int>()->default_value(1), "Number of dealers to send messages")
			("threads,t", value<int>()->default_value(1), "Threads per dealer")
			("messages,m", value<int>()->default_value(1), "Messages per dealer")
		;

		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return EXIT_SUCCESS;
		}
		
		create_client(vm["dealers"].as<int>(), vm["threads"].as<int>(), vm["messages"].as<int>());
		return EXIT_SUCCESS;
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
