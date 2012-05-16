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

#ifndef _COCAINE_DEALER_COCAINE_ENDPOINT_HPP_INCLUDED_
#define _COCAINE_DEALER_COCAINE_ENDPOINT_HPP_INCLUDED_

#include <string>

namespace cocaine {
namespace dealer {

 // predeclaration
class cocaine_endpoint {
public:
	cocaine_endpoint() {}
	~cocaine_endpoint() {}

	std::string endpoint_;
	std::string route_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_COCAINE_ENDPOINT_HPP_INCLUDED_
