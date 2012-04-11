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

#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <msgpack.hpp>

#include "cocaine/dealer/dealer.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"
#include <cocaine/dealer/utils/data_container.hpp>
#include <cocaine/dealer/utils/error.hpp>

using namespace cocaine::dealer;
namespace po = boost::program_options;

std::string config_path = "tests/config_example.json";

// create client
boost::shared_ptr<client> client_ptr;

int messages_count = 0;
int responces_count = 0;

void worker() {
	// prepare data
	std::map<std::string, int> val;
	val["uid"] = 123;
	val["score"] = 1;
	val["service"] = 1;

	msgpack::sbuffer buffer;
	msgpack::pack(buffer, val);

	//message path
	message_path path;
	path.service_name = "karma-tests";
	path.handle_name = "event";

	// message policy
	message_policy policy;

	for (int i = 0; i < messages_count; ++i) {
		response resp = client_ptr->send_message(buffer.data(), buffer.size(), path, policy);

		try {
			//data_container data;
			//resp.get(&data);

			//++responces_count;
			//if (responces_count % 1000 == 0) {
			//	std::cout << "processed: " << responces_count << std::endl;
			//}

			/*
			msgpack::unpacked msg;
			msgpack::unpack(&msg, (const char*)data.data(), data.size());
			msgpack::object obj = msg.get();
			std::cout << "resp data: " << obj << std::endl;
			*/
		}
		catch (const error& err) {
			const int t = err.type();
			std::cout << "error type: " << t << ", error message: " << err.what() << std::endl;
		}
		catch (const std::exception& ex) {
			std::cout << "error message: " << ex.what() << std::endl;
		}
		catch (...) {
			std::cout << "caught exception, no error message." << std::endl;
		}
	}
}

void create_client(int add_messages_count) {
	messages_count = add_messages_count;
	client_ptr.reset(new client(config_path));

	int pool_size = 1;
	boost::thread pool[pool_size];

	progress_timer timer;

	// create threads
	for (int i = 0; i < pool_size; ++i) {
		pool[i] = boost::thread(&worker);
	}

	sleep(10);

	// wait for them to finish
	for (int i = 0; i < pool_size; ++i) {
		pool[i].join();
	}

	std::cout << "elapsed: " << timer.elapsed().as_double() << std::endl;
}

int
main(int argc, char** argv) {
	try {
		po::options_description desc("Allowed options");
		desc.add_options()
			("help", "Produce help message")
			("messages,m", po::value<int>(), "Add messages to server")
		;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

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
