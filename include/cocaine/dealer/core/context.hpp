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

#ifndef _COCAINE_DEALER_CONTEXT_HPP_INCLUDED_
#define _COCAINE_DEALER_CONTEXT_HPP_INCLUDED_

#include <string>
#include <map>
#include <memory>

#include <zmq.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#include "cocaine/dealer/core/configuration.hpp"
//#include "cocaine/dealer/core/statistics_collector.hpp"
#include "cocaine/dealer/utils/smart_logger.hpp"

namespace cocaine {
namespace dealer {

class eblob_storage;

class context_t : private boost::noncopyable {
public:
	explicit context_t(const std::string& config_path);
	virtual ~context_t();

	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config();
	boost::shared_ptr<zmq::context_t> zmq_context();
	//boost::shared_ptr<statistics_collector> stats();
	boost::shared_ptr<eblob_storage> storage();

private:
	boost::shared_ptr<zmq::context_t> zmq_context_m;
	boost::shared_ptr<base_logger> logger_m;
	boost::shared_ptr<configuration> config_m;
	//boost::shared_ptr<statistics_collector> stats_m;
	boost::shared_ptr<eblob_storage> storage_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CONTEXT_HPP_INCLUDED_
