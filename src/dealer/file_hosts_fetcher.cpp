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

#include <sys/stat.h>	
#include <unistd.h>
#include <time.h>

#include <stdexcept>
#include <fstream>
#include <iostream>

#include <boost/current_function.hpp>
#include <boost/tokenizer.hpp>

#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"

namespace cocaine {
namespace dealer {

file_hosts_fetcher::file_hosts_fetcher(service_info_t service_info,
									   boost::uint32_t interval) :
	service_info_(service_info),
	interval_(interval),
	file_modification_time_(0)
{
	start();
}

file_hosts_fetcher::~file_hosts_fetcher() {
	stop();
}

// passes list of hosts to callback
void
file_hosts_fetcher::set_callback(boost::function<void(std::vector<host_info_t>&, service_info_t)> callback) {
	callback_ = callback;
}

void
file_hosts_fetcher::start() {
	refresher_.reset(new refresher(boost::bind(&file_hosts_fetcher::interval_func, this), interval_));
}

void
file_hosts_fetcher::stop() {
	refresher_.reset(NULL);
}

void
file_hosts_fetcher::interval_func() {
	std::string buffer;

	// check file modification time
	struct stat attrib;
	stat(service_info_.hosts_file_.c_str(), &attrib);

	if (attrib.st_mtime <= file_modification_time_) {
		return;
	}

	// load file
	std::string code;
	std::ifstream file;
	file.open(service_info_.hosts_file_.c_str(), std::ifstream::in);

	if (!file.is_open()) {
		return;
	}

	size_t max_size = 512;
	char buff[max_size];

	while (!file.eof()) {		
		memset(buff, 0, sizeof(buff));
		file.getline(buff, max_size);
		buffer += buff;
		buffer += "\n";
	}

	file.close();
	
	// get hosts from received data
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("\n");
	tokenizer tokens(buffer, sep);
	
	std::vector<host_info_t> hosts;
	for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
		try {
			host_info_t host(*tok_iter);
			hosts.push_back(host);
		}
		catch (...) {
		}
	}
	
	callback_(hosts, service_info_);
}

} // namespace dealer
} // namespace cocaine
