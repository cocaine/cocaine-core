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
		m_logging_enabled(true) {}

	dealer_object_t(const boost::shared_ptr<context_t>& ctx, bool logging_enabled) :
		m_ctx(ctx),
		m_logging_enabled(logging_enabled) {}

	void set_context(const boost::shared_ptr<context_t>& ctx) {
		m_ctx = ctx;
	}

	void log(const std::string& message, ...) {
		if (!m_logging_enabled || !m_ctx || !(m_ctx->logger())) {
			return;
		}

		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message.c_str(), vl);
		va_end(vl);

		m_ctx->logger()->log(std::string(buff));
	}

	void log(unsigned int type, const std::string& message, ...) {
		if (!m_logging_enabled || !m_ctx || !(m_ctx->logger())) {
			return;
		}

		char buff[2048];
		memset(buff, 0, sizeof(buff));
	
		va_list vl;
		va_start(vl, message);
		vsnprintf(buff, sizeof(buff) - 1, message.c_str(), vl);
		va_end(vl);

		m_ctx->logger()->log(type, std::string(buff));
	}

	boost::shared_ptr<context_t> context() const {
		return m_ctx;
	}

	boost::shared_ptr<configuration_t> config() const {
		return m_ctx->config();
	}

private:
	boost::shared_ptr<context_t> m_ctx;
	bool m_logging_enabled;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_OBJECT_HPP_INCLUDED_
