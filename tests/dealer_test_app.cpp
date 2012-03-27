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
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/any.hpp>

#include <msgpack.hpp>

#include <eblob/eblob.hpp>

#include "cocaine/dealer/client.hpp"

#include "cocaine/dealer/utils/persistent_data_container.hpp"
#include "cocaine/dealer/utils/data_container.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"
#include "cocaine/dealer/storage/eblob_storage.hpp"

#include <sys/time.h>

namespace po = boost::program_options;
namespace cd = cocaine::dealer;

std::string config_path = "tests/config_example.json";
int msg_counter = 0;

int test_client(int argc, char** argv);
void test_data_container();
void test_eblob_storage();
void test_experiment();
void show_contents(const cocaine::dealer::data_container& c);
void create_client(int add_messages_count);

int
main(int argc, char** argv) {
	//test_experiment();
	//test_eblob_storage();
	//test_data_container();
	return test_client(argc, argv);

	return EXIT_SUCCESS;
}

cd::progress_timer timer;

void test_experiment() {
		// create eblob logger
		zbr::eblob_logger elog("/var/tmp/eblobs/log", 0);

		// create config
		int64_t blob_size = 2147483648;  // 2gb
        zbr::eblob_config cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.file = (char*)"/var/tmp/eblobs/test1";
        cfg.log = elog.log();
        cfg.sync = 30;
        cfg.blob_size = blob_size;
        cfg.defrag_timeout = -1;
        cfg.iterate_threads = 1;

        // create eblob
        zbr::eblob blob_a(&cfg);

        // write data
        std::string str = "If you do not want your code to be platform dependent, you may stick with more portable solutions";
        
        srand(time(NULL));
        std::stringstream sstr;

		timeval begin, end;
		gettimeofday(&begin, NULL);
        for (int i = 0; i < 10000; ++i) {
        	sstr.str("");
        	sstr << rand() << "_postfix";
        	blob_a.write_hashed(sstr.str(), str, 0, BLOB_DISK_CTL_OVERWRITE, 0);
        	blob_a.write_hashed(sstr.str(), str, 0, BLOB_DISK_CTL_OVERWRITE, 1);
    	}
    	gettimeofday(&end, NULL);

    	double beg_time = ((double)begin.tv_sec + begin.tv_usec / 1000000.0);
    	double end_time = ((double)end.tv_sec + end.tv_usec / 1000000.0);

    	std::cout << std::fixed << std::setprecision(10) << "elapsed secs: " << end_time - beg_time << std::endl;
    	std::cout << "records count in blob: " << blob_a.elements() << std::endl;
}

int prev = -1;

void response_callback(const cd::response& response, const cd::response_info& info) {
	if (info.error != cd::MESSAGE_CHOKE) {
		//std::cout << "resp (CHUNK) uuid: " << response.uuid << std::endl;
		//std::string st((const char*)response.data, response.size);
		//std::cout << "resp data: " << st << std::endl;

		/*
		std::string st((const char*)response.data, response.size);

		// deserialize it.
        msgpack::unpacked msg;
        msgpack::unpack(&msg, (const char*)response.data, response.size);
 
        // print the deserialized object.
        msgpack::object obj = msg.get();
        std::cout << "resp data: " << obj << std::endl;
 		*/
 		/*
        // convert it into statically typed object.
        std::map<std::string, int> m;
        obj.convert(&m);

        std::map<std::string, int>::iterator it = m.begin();
        std::cout << it->first << " : " << it->second << std::endl;
        */
	}
	else {
		++msg_counter;
		//std::cout << "resp (CHOKE) uuid: " << response.uuid << std::endl;
		//std::cout << "resp done! " << msg_counter << std::endl;
		//++msg_counter;
		//std::cout << "good responces: " << msg_counter << "\n";
		if (msg_counter == 100000) {
			std::cout << "elapsed: " << timer.elapsed().as_double() << " secs" << std::endl;
		}
	}

	//if (msg_counter % 100 == 0) {
	//	if (msg_counter / 100 > prev) {
	//		std::cout << "received " << msg_counter << " responces" << std::endl;
	//		prev = msg_counter / 100;
	//	}
	//}
}

void create_client(int add_messages_count) {
	// create py app message path
	cd::message_path path_py;
	path_py.service_name = "karma-tests";
	path_py.handle_name = "event";

	cd::client c(config_path);
	c.connect();
	c.set_response_callback(boost::bind(&response_callback, _1, _2), path_py);
	sleep(5);

	//tv.init_from_current_time();

	std::map<std::string, int> val;
	val["uid"] = 123;
	val["score"] = 1;
	val["service"] = 1;

	msgpack::sbuffer buffer;
	msgpack::pack(buffer, val);

	cd::progress_timer timer;

	// send message to py app
	timer.reset();

	for (int i = 0; i < add_messages_count; ++i) {
		std::string uuid1 = c.send_message(buffer.data(), buffer.size(), path_py);
	}

	std::cout << std::fixed << std::setprecision(6) << "elapsed secs: " << timer.elapsed().as_double() << std::endl;

	sleep(600);

	/*
	// create py app message path
	cd::message_path path_py;
	path_py.service_name = "cox";
	path_py.handle_name = "worker3";

	cd::client c(config_path);
	c.connect();
	c.set_response_callback(boost::bind(&response_callback, _1, _2), path_py);

	sleep(4);

	// send message to py app
	for (int i = 0; i < add_messages_count; ++i) {
		std::string message = "{ \"key\" : \"some value\" }";
		std::string uuid1 = c.send_message(message, path_py);
		std::cout << "mesg uuid: " << uuid1 << std::endl;
	}
	*/
	// send message to perl app
	//for (int i = 0; i < add_messages_count; ++i) {
	//	std::string message = "http://longcat.ru";
	//	std::string uuid1 = c.send_message(message, path_perl);
	//	std::cout << "mesg uuid: " << uuid1 << std::endl;
	//}
}

void test_eblob_storage() {
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
}

int test_client(int argc, char** argv) {
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
