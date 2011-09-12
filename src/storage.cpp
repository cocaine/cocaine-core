#include "storage.hpp"

#include "storage/void.hpp"
#include "storage/files.hpp"
#include "storage/mongo.hpp"

using namespace cocaine::storage;

boost::shared_ptr<abstract_storage_t> storage_t::instance() {
    if(!g_object.get()) {
        std::string driver = config_t::get().storage.driver;

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

boost::shared_ptr<abstract_storage_t> storage_t::g_object;
