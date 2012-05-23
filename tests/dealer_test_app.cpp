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
#include <iomanip>
#include <sstream>
#include <map>
#include <time.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/flyweight.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/min.hpp>

#include <msgpack.hpp>

#include "cocaine/dealer/dealer.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"
#include <cocaine/dealer/utils/data_container.hpp>
#include <cocaine/dealer/utils/error.hpp>
#include <cocaine/dealer/utils/smart_logger.hpp>
#include <cocaine/dealer/utils/networking.hpp>

#include "cocaine/dealer/cocaine_node_info/cocaine_node_info_parser.hpp"
#include "cocaine/dealer/cocaine_node_info/cocaine_node_info.hpp"

#include <cocaine/dealer/core/configuration.hpp>
#include "cocaine/dealer/core/cocaine_endpoint.hpp"

using namespace cocaine::dealer;
using namespace boost::program_options;
using namespace boost::accumulators;

std::string config_path = "tests/config_example.json";
boost::shared_ptr<client> client_ptr;

int messages_count = 0;
volatile int slow_messages_count = 0;

boost::mutex mutex;

void worker() {
	accumulator_set<float, features<tag::min, tag::max, tag::mean, tag::median> > accum;

	message_path path("rimz_app", "rimz_func");
	message_policy policy;
	policy.deadline = 1.0;
	policy.max_retries = -1;
	std::string payload = "response chunk: ";

	for (int i = 0; i < messages_count; ++i) {
		progress_timer t;

		//boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

		try {
			boost::shared_ptr<response> resp;

			if (client_ptr) {
				resp = client_ptr->send_message(payload.data(), payload.size(), path, policy);
			}

			data_container data;
			//resp->get(&data, 0.000175);

			//accum(t.elapsed().as_double());
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

		if (t.elapsed().as_double() > 0.200) {
			++slow_messages_count;
			//std::cout << "slow time: " << t.elapsed().as_double() << ", num:" << slow_messages_count << "\n";
		}
	}

	//boost::mutex::scoped_lock lock(mutex);
	//std::cout << std::fixed << std::setprecision(6);
	//std::cout << "min - " << boost::accumulators::min(accum);
	//std::cout << "\tmax - " << boost::accumulators::max(accum);
	//std::cout << "\tmean - " << boost::accumulators::mean(accum);
	//std::cout << " \tmedian - " << boost::accumulators::median(accum) << "\n" << std::flush;
}

void create_client(int add_messages_count) {
	const int pool_size = 200;
	
	std::cout << "----------------------------------- test info -------------------------------------------\n";
	std::cout << "sending " << add_messages_count * pool_size << " messages using " << pool_size << " threads\n";
	std::cout << "-----------------------------------------------------------------------------------------\n";
	
	messages_count = add_messages_count;
	
	client_ptr.reset(new client(config_path));

	boost::thread pool[pool_size];

	progress_timer timer;

	// create threads
	std::cout << "sending messages...\n";

	for (int i = 0; i < pool_size; ++i) {
		pool[i] = boost::thread(&worker);
	}

	// wait for them to finish
	for (int i = 0; i < pool_size; ++i) {
		pool[i].join();
	}

	std::cout << "sending messages done.\n";
	sleep(20);
	client_ptr.reset();

	std::cout << "----------------------------------- test results ----------------------------------------\n";
	std::cout << "elapsed: " << timer.elapsed().as_double() << std::endl;
	std::cout << "approx performance: " << (add_messages_count * pool_size) / timer.elapsed().as_double() << " rps." << std::endl;
}

int
main(int argc, char** argv) {
	try {
		options_description desc("Allowed options");
		desc.add_options()
			("help", "Produce help message")
			("messages,m", value<int>(), "Add messages to server")
		;

		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return EXIT_SUCCESS;
		}

		if (vm.count("messages")) {
			int add_messages_count = vm.count("messages") ? vm["messages"].as<int>() : 0;
			create_client(add_messages_count);

			return EXIT_SUCCESS;
		}

		std::cout << desc << std::endl;
		return EXIT_FAILURE;
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
