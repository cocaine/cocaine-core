#include "cocaine/storages/abstract.hpp"
#include "cocaine/storages/files.hpp"
#include "cocaine/storages/mongo.hpp"
#include "cocaine/storages/void.hpp"

using namespace cocaine::storage;

boost::shared_ptr<storage_t> storage_t::create() {
    boost::shared_ptr<storage_t> object;
    std::string driver(config_t::get().storage.driver);

    if(driver == "files") {
        object.reset(new backends::file_storage_t());
    } else if(driver == "mongo") {
        object.reset(new backends::mongo_storage_t());
    } else {
        object.reset(new backends::void_storage_t());
    }

    return object;
}

