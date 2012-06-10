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

#ifndef _COCAINE_DEALER_OBJECT_HPP_INCLUDED_
#define _COCAINE_DEALER_OBJECT_HPP_INCLUDED_

#include <string>

#include <boost/shared_ptr.hpp>

#include "cocaine/dealer/core/context.hpp"

namespace cocaine {
namespace dealer {

class dealer_object_t {
public:
	dealer_object_t() :
		logging_enabled_m(true) {}

	dealer_object_t(const boost::shared_ptr<context_t>& ctx, bool logging_enabled) :
		ctx_m(ctx),
		logging_enabled_m(logging_enabled) {}

	void set_context(const boost::shared_ptr<context_t>& ctx) {
		ctx_m = ctx;
	}

	void log(const std::string& message, ...) {
		if (!logging_enabled_m || !ctx_m || !(ctx_m->logger())) {
			return;
		}

		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message.c_str(), vl);
		va_end(vl);

		ctx_m->logger()->log(std::string(buff));
	}

	void log(unsigned int type, const std::string& message, ...) {
		if (!logging_enabled_m || !ctx_m || !(ctx_m->logger())) {
			return;
		}

		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message.c_str(), vl);
		va_end(vl);

		ctx_m->logger()->log(type, std::string(buff));
	}

	boost::shared_ptr<context_t> context() const {
		return ctx_m;
	}

	boost::shared_ptr<configuration> config() const {
		return ctx_m->config();
	}

private:
	boost::shared_ptr<context_t> ctx_m;
	bool logging_enabled_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_OBJECT_HPP_INCLUDED_
