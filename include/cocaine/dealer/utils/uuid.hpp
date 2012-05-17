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

#ifndef _COCAINE_DEALER_UUID_HPP_INCLUDED_
#define _COCAINE_DEALER_UUID_HPP_INCLUDED_

#include <uuid/uuid.h>

namespace cocaine {
namespace dealer {

class wuuid_t {
public:
	wuuid_t() {
		clear();
	}

	wuuid_t(const wuuid_t& rhs) :
		uuid_string_(rhs.uuid_string_)
	{
		uuid_copy(uuid_data_, rhs.uuid_data_);
	}

	wuuid_t(const uuid_t& rhs) {
		uuid_copy(uuid_data_, rhs);
		generate_string();
	}

	wuuid_t(const std::string& uuid_string) {
		if (0 == uuid_parse(uuid_string.c_str(), uuid_data_)) {
			uuid_string_ = uuid_string;
		}
		else {
			clear();
		}
	}

	wuuid_t(const char* uuid_string) {
		if (0 == uuid_parse(uuid_string, uuid_data_)) {
			uuid_string_ = uuid_string;
		}
		else {
			clear();
		}
	}

	~wuuid_t() {}

	bool is_null() const {
		return (uuid_is_null(uuid_data_) == 1 ? true : false);
	}

	void clear() {
		uuid_clear(uuid_data_);
		generate_string();
	}

	const std::string& generate() {
		generate_data();
		generate_string();
		return uuid_string_;
	}

	const std::string& str() const {
		return uuid_string_;
	}

	const uuid_t& data() const {
		return uuid_data_;
	}

	bool operator == (const wuuid_t& rhs) const {
		return (uuid_compare(uuid_data_, rhs.uuid_data_) == 0 ? true : false);
	}

	bool operator != (const wuuid_t& rhs) const {
		return (!(*this == rhs));
	}

	bool operator == (const std::string& rhs) const {
		wuuid_t tmp_rhs(rhs);
		return (*this == tmp_rhs);
	}

	bool operator != (const std::string& rhs) const {
		wuuid_t tmp_rhs(rhs);
		return (*this != tmp_rhs);
	}

	bool operator == (const char* rhs) const {
		wuuid_t tmp_rhs(rhs);
		return (*this == tmp_rhs);
	}

	bool operator != (const char* rhs) const {
		wuuid_t tmp_rhs(rhs);
		return (*this != tmp_rhs);
	}

private:
	void generate_data() {
		uuid_generate(uuid_data_);
	}

	void generate_string() {
		char buff[37];
		memset(buff, 0, sizeof(buff));
		uuid_unparse(uuid_data_, buff);

		uuid_string_ = buff;
	}

	uuid_t uuid_data_;
	std::string uuid_string_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_UUID_HPP_INCLUDED_
