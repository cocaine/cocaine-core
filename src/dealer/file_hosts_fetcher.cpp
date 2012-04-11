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

file_hosts_fetcher::file_hosts_fetcher(service_info_t service_info) :
	service_info_(service_info),
	file_modification_time_(0)
{
}

file_hosts_fetcher::~file_hosts_fetcher() {
}

void
file_hosts_fetcher::get_hosts(std::vector<host_info_t>& hosts, service_info_t& service_info) {
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

	for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
		try {
			host_info_t host(*tok_iter);
			hosts.push_back(host);
		}
		catch (...) {
		}
	}
	
	service_info = service_info_;
}

} // namespace dealer
} // namespace cocaine
