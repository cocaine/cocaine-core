/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <sys/stat.h>	
#include <unistd.h>
#include <time.h>

#include <stdexcept>
#include <fstream>
#include <iostream>

#include <boost/current_function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <cocaine/dealer/utils/error.hpp>
#include "cocaine/dealer/heartbeats/file_hosts_fetcher.hpp"

namespace cocaine {
namespace dealer {

file_hosts_fetcher::file_hosts_fetcher(const service_info_t& service_info) :
	service_info_m(service_info),
	file_modification_time_m(0)
{
}

file_hosts_fetcher::~file_hosts_fetcher() {
}

bool
file_hosts_fetcher::get_hosts(inetv4_endpoints& endpoints, service_info_t& service_info) {
	std::string buffer;

	// check file modification time
	struct stat attrib;
	stat(service_info_m.hosts_source_.c_str(), &attrib);

	if (attrib.st_mtime <= file_modification_time_m) {
		return false;
	}

	// load file
	std::string code;
	std::ifstream file;
	file.open(service_info_m.hosts_source_.c_str(), std::ifstream::in);

	if (!file.is_open()) {
		throw internal_error("hosts file: " + service_info_m.hosts_source_ + " failed to open at: " + std::string(BOOST_CURRENT_FUNCTION));
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
			std::string line = *tok_iter;

			// look for ip/port parts
			size_t where = line.find_last_of(":");

			if (where == std::string::npos) {
				endpoints.push_back(inetv4_endpoint(inetv4_host(line)));
			}
			else {
				std::string ip = line.substr(0, where);
				std::string port = line.substr(where + 1, (line.length() - (where + 1)));

				endpoints.push_back(inetv4_endpoint(ip, port));
			}
		}
		catch (...) {
		}
	}
	
	service_info = service_info_m;
	return true;
}

} // namespace dealer
} // namespace cocaine
