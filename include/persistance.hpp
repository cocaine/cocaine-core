#ifndef YAPPI_PERSISTANCE_HPP
#define YAPPI_PERSISTANCE_HPP

#include "common.hpp"
#include "file_storage.hpp"
#include "eblob_storage.hpp"

namespace yappi { namespace persistance {

template<class Backend>
class storage_facade_t: public Backend {
    public:
        storage_facade_t(const std::string& uuid):
            Backend(uuid)
        {}

    public:
        bool put(const std::string& key, const Json::Value& value) {
            return static_cast<Backend*>(this)->put(key, value);
        }

        bool exists(const std::string& key) {
            return static_cast<Backend*>(this)->exists(key);
        }

        Json::Value get(const std::string& key) {
            return static_cast<Backend*>(this)->get(key);
        }

        Json::Value all() {
            return static_cast<Backend*>(this)->all();
        }

        void remove(const std::string& key) {
            return static_cast<Backend*>(this)->remove(key);
        }
};

typedef storage_facade_t<backends::eblob_storage_t> storage_t;

}}

#endif
