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
#include <boost/heap/priority_queue.hpp>

#include <msgpack.hpp>

#include "cocaine/dealer/dealer.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"

namespace po = boost::program_options;
namespace cd = cocaine::dealer;

cd::progress_timer timer;
std::string config_path = "tests/config_example.json";

void create_client(int add_messages_count) {
	
	// prepare data
	std::map<std::string, int> val;
	val["uid"] = 123;
	val["score"] = 1;
	val["service"] = 1;

	msgpack::sbuffer buffer;
	msgpack::pack(buffer, val);

	//message path
	cd::message_path path;
	path.service_name = "karma-tests";
	path.handle_name = "event";

	// message policy
	cd::message_policy policy;

	// create client
	cd::client client(config_path);
	//cd::response resp = client.send_message(buffer.data(), buffer.size(), path, policy);

	/*
	int code;
	while (resp.get(code)) {
		switch (resp.code()) {
			case cd::response_code::message_chunk:
				std::cout << "got chunk!" << std::endl;
				break;

			case cd::response_code::message_choke:
				std::cout << "got choke!" << std::endl;
				break;
		}
	}
	*/

	//std::cout << "got bad response!" << std::endl;

	/*
	c.set_response_callback(boost::bind(&response_callback, _1, _2), path_py);
	sleep(6);


	timer.reset();

	// send messages to py app
	std::cout << "sending " << add_messages_count << " messages...\n";
	for (int i = 0; i < add_messages_count; ++i) {
		std::string uuid1 = c.send_message(buffer.data(), buffer.size(), path_py);
	}

	sleep(600);
	*/
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
