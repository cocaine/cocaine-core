//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/storages/base.hpp"
#include "cocaine/storages/files.hpp"
#include "cocaine/storages/mongo.hpp"
#include "cocaine/storages/void.hpp"

using namespace cocaine::storage;

boost::shared_ptr<storage_t> storage_t::create() {
    boost::shared_ptr<storage_t> object;
    std::string driver(config_t::get().storage.driver);

    if(driver == "files") {
        object.reset(new file_storage_t());
    } else if(driver == "mongo") {
        object.reset(new mongo_storage_t());
    } else {
        object.reset(new void_storage_t());
    }

    return object;
}

