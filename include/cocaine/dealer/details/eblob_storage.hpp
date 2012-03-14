//
// Copyright (C) 2011-2012 Rim Zaidullin <tinybit@yandex.ru>
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

#ifndef _COCAINE_DEALER_EBLOB_STORAGE_HPP_INCLUDED_
#define _COCAINE_DEALER_EBLOB_STORAGE_HPP_INCLUDED_

#include <string>
#include <map>
#include <memory>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

namespace cocaine {
namespace dealer {

class eblob_storage : private boost::noncopyable {
public:
	explicit eblob_storage() {};
	virtual ~eblob_storage() {};

private:
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_EBLOB_STORAGE_HPP_INCLUDED_
