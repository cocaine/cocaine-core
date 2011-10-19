#include "cocaine/storage.hpp"
#include "cocaine/storages/void.hpp"
#include "cocaine/storages/files.hpp"
#include "cocaine/storages/mongo.hpp"

using namespace cocaine::storage;

boost::shared_ptr<abstract_storage_t> storage_t::instance() {
    if(!g_object.get()) {
        std::string driver(config_t::get().storage.driver);

        if(driver == "files") {
            g_object.reset(new backends::file_storage_t());
        } else if(driver == "mongo") {
            g_object.reset(new backends::mongo_storage_t());
        } else {
            g_object.reset(new backends::void_storage_t());
        }
    }

    return g_object;
}

storage_t::storage_t() { }

boost::shared_ptr<abstract_storage_t> storage_t::g_object;
