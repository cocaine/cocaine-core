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
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <msgpack.hpp>


#include "cocaine/dealer/client.hpp"
#include "cocaine/dealer/details/time_value.hpp"
#include "cocaine/dealer/details/eblob_storage.hpp"
#include "cocaine/dealer/details/persistent_data_container.hpp"
#include "cocaine/dealer/details/data_container.hpp"
#include "cocaine/dealer/details/smart_logger.hpp"

namespace po = boost::program_options;
namespace cd = cocaine::dealer;

std::string config_path = "tests/config_example.json";

void response_callback(const cd::response& response, const cd::response_info& info) {
	if (info.error != cd::MESSAGE_CHOKE) {
		std::cout << "resp (CHUNK) uuid: " << response.uuid << std::endl;
		std::string st((const char*)response.data, response.size);
		std::cout << "resp data: " << st << std::endl;
	}
	else {
		std::cout << "resp (CHOKE) uuid: " << response.uuid << std::endl;
		std::cout << "resp done!" << std::endl;
	}
}

void create_client(int add_messages_count) {
	// create py app message path
	cd::message_path path_py;
	path_py.service_name = "python";
	path_py.handle_name = "worker3";

	// create perl app message path
	cd::message_path path_perl;
	path_perl.service_name = "perl-testing";
	path_perl.handle_name = "test_handle";

	cd::client c(config_path);
	c.connect();
	c.set_response_callback(boost::bind(&response_callback, _1, _2), path_py);
	c.set_response_callback(boost::bind(&response_callback, _1, _2), path_perl);

	sleep(3);

	// send message to py app
	for (int i = 0; i < add_messages_count; ++i) {
		std::string message = "{ \"key\" : \"some value\" }";
		std::string uuid1 = c.send_message(message, path_py);
		std::cout << "mesg uuid: " << uuid1 << std::endl;
	}

	// send message to perl app
	for (int i = 0; i < add_messages_count; ++i) {
		std::string message = "http://longcat.ru";
		std::string uuid1 = c.send_message(message, path_perl);
		std::cout << "mesg uuid: " << uuid1 << std::endl;
	}

	sleep(5);
}

void show_contents(const cocaine::dealer::data_container& c) {
	if (c.data()) {
		std::string str((const char*)c.data(), 0, c.size());
		std::cout << str << std::endl;
	}
	else {
		std::cout << "- no data, ";

		if (c.empty()) {
			std::cout << " empty container -";	
		}
		else {
			std::cout << " non-empty container -";
		}

		std::cout << std::endl;
	}
}

int
main(int argc, char** argv) {
	/*
	using namespace cocaine::dealer;

	boost::shared_ptr<base_logger> logger;
	logger.reset(new smart_logger<stdout_logger>());

	eblob_storage st("/var/tmp/eblobs");
	st.open_eblob("test");

	std::string test_data = "Люди в тюрьме меньше времени сидят, чем вы на Лепре, mcore.";
	persistant_data_container dc(test_data.data(), test_data.size());
	dc.set_eblob(st["test"], "container test key");
	dc.commit_data();
	dc.load_data();

	if (dc.is_data_in_memory()) {
		logger->log("%s", std::string((char*)dc.data(), 0, dc.size()).c_str());
	}
	else {
		logger->log("data is not in memory, load first");
	}
	*/
	//st["test"].write("huita", "poebta");
	//st["test"].write("aaaa", "sdfhb");
	//st["test"].write("aaaa", "VAL", 1);

	//st.write("huita", "poebta", 1);
	//logger->log("%llu", st["test"].items_count());
	//logger->log("%s", st["test"].read("aaaa", 1).c_str());
	//st["est"].write("aaaa", "VAL", 1);

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

void test_data_container() {
	using namespace cocaine::dealer;

	std::string str1 = "this is some test data right here.";
	std::string str2 = "this is some other test data.";

	data_container dc1(str1.data(), str1.size());
	//show_contents(dc1);

	data_container dc2;
	dc2 = dc1;
	//show_contents(dc2);

	dc2.clear();
	//show_contents(dc2);
	//show_contents(dc1);

	dc1.clear();
	//show_contents(dc1);

	dc1.set_data(str2.data(), str2.size());
	//show_contents(dc1);

	data_container dc3;
	//show_contents(dc3);

	dc1 = dc3;
	//show_contents(dc1);
	//show_contents(dc3);

	dc1.set_data(str1.data(), str1.size());
	dc2.set_data(str1.data(), str1.size());
	dc3.set_data(str2.data(), str2.size());

	if (dc1 == dc2) {
		std::cout << "test 1 passed\n";
	}

	if (dc2 == dc1) {
		std::cout << "test 2 passed\n";
	}

	if (dc1 != dc3) {
		std::cout << "test 3 passed\n";
	}

	if (dc3 != dc1) {
		std::cout << "test 4 passed\n";
	}

	dc2.set_data(NULL, 0);

	if (dc2 != dc1) {
		std::cout << "test 5 passed\n";
	}

	if (dc1 != dc2) {
		std::cout << "test 6 passed\n";
	}
}
