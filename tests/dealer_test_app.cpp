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
#include <sstream>

#include <boost/program_options.hpp>
#include <boost/bind.hpp> 
#include <boost/filesystem.hpp> 

#include <msgpack.hpp>

#include "cocaine/dealer/client.hpp"
#include "cocaine/dealer/details/time_value.hpp"

namespace po = boost::program_options;
namespace cd = cocaine::dealer;

std::string config_path = "tests/config_example.json";

/*
void response_callback(const cd::response& response, const cd::response_info& info) {
	if (info.error != cd::MESSAGE_CHOKE) {

		std::cout << "resp (CHUNK) uuid: " << response.uuid << std::endl;
		//msgpack::unpacked msg;
		//msgpack::unpack(&msg, (const char*)response.data, response.size);
		//msgpack::object obj = msg.get();
		//std::stringstream stream;

		std::string st((const char*)response.data, response.size);

		std::cout << "resp data: " << st << std::endl;
	}
	else {
		//++count;
		//if (count % 1000 == 0) {
		//	std::cout << "count: " << count << std::endl;
		//}

		std::cout << "resp (CHOKE) uuid: " << response.uuid << std::endl;
		std::cout << "resp done!" << std::endl;
	}
}
*/
void create_client(int add_messages_count) {
	/*
	// create message path
	cd::message_path path;
	path.service_name = "perl-testing";
	path.handle_name = "test_handle";

	cd::client c(config_path);
	c.connect();
	c.set_response_callback(boost::bind(&response_callback, _1, _2), path);

	sleep(3);

	// send message
	for (int i = 0; i < add_messages_count; ++i) {
		std::string message = "http://longcat.ru";
		std::string uuid1 = c.send_message(message, path);
		std::cout << "mesg uuid: " << uuid1 << std::endl;
	}
	*/
	/*
	//lsd::time_value tv;
	tv.init_from_current_time();

	// create message policy
	lsd::message_policy policy;
	policy.deadline = tv.as_double() + 10.0;

	// create message data
	std::map<std::string, int> event;
	event["service"] = 1;
	event["uid"] = 12345;
	event["score"] = 500;

	msgpack::sbuffer buffer;
	msgpack::pack(buffer, event);

	std::string message = "whoa!";

	// send messages
	for (int i = 0; i < add_messages_count; ++i) {
		std::string uuid1 = c.send_message(message, path);
		std::cout << "mesg uuid: " << uuid1 << std::endl;
	}
	*/

	sleep(5);
}

int
main(int argc, char** argv) {
	try {
		po::options_description desc("Allowed options");
		desc.add_options()
			("help", "Produce help message")
			("client,c", "Start as server")
			("messages,m", po::value<int>(), "Add messages to server")
		;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return EXIT_SUCCESS;
		}

		if (vm.count("client")) {
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
