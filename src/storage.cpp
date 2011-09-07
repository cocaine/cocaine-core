#include "storage.hpp"

#include "detail/void.hpp"
#include "detail/files.hpp"
#include "detail/mongo.hpp"

using namespace yappi::storage;

boost::shared_ptr<abstract_storage_t> storage_t::instance() {
    if(!object.get()) {
        std::string type = config_t::get().storage.type;

        if(type == "files") {
            object.reset(new backends::file_storage_t());
        } else if(type == "mongo") {
            object.reset(new backends::mongo_storage_t());
        } else {
            object.reset(new backends::void_storage_t());
        }
    }

    return object;
}

boost::shared_ptr<abstract_storage_t> storage_t::object;
