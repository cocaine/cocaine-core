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

#ifndef _COCAINE_INTERNAL_ERROR_HPP_INCLUDED_
#define _COCAINE_INTERNAL_ERROR_HPP_INCLUDED_

#include <cocaine/dealer/utils/error/basic_error.hpp>
#include <cocaine/dealer/types.hpp>

namespace cocaine {
namespace dealer {

class internal_error : public basic_error {
public:
	internal_error(const std::string& format, ...) :
		basic_error()
	{
		va_list args;
		va_start(args, format);
		vsnprintf(message_, ERROR_MESSAGE_SIZE, format.c_str(), args);
		va_end(args);
	}

	virtual ~internal_error() throw () {
	}

	internal_error(internal_error const& other) : basic_error() {
		memcpy(message_, other.message_, ERROR_MESSAGE_SIZE);
	}
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_INTERNAL_ERROR_HPP_INCLUDED_
