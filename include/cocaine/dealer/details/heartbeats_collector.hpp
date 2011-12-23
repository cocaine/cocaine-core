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

#ifndef _LSD_HEARTBEATS_COLLECTOR_HPP_INCLUDED_
#define _LSD_HEARTBEATS_COLLECTOR_HPP_INCLUDED_

#include <string>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/bind.hpp>

#include "cocaine/dealer/structs.hpp"

#include "cocaine/dealer/details/host_info.hpp"
#include "cocaine/dealer/details/handle_info.hpp"
#include "cocaine/dealer/details/smart_logger.hpp"
#include "cocaine/dealer/details/configuration.hpp"

namespace lsd {

class heartbeats_collector {
public:
	typedef boost::function<void(const service_info_t&, const std::vector<host_info_t>&, const std::vector<handle_info_t>&)> callback_t;

	virtual void run() = 0;
	virtual void stop() = 0;
	virtual void set_callback(callback_t callback) = 0;
	virtual void set_logger(boost::shared_ptr<base_logger> logger) = 0;
};

} // namespace lsd

#endif // _LSD_HEARTBEATS_COLLECTOR_HPP_INCLUDED_
