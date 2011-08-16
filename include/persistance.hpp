#ifndef YAPPI_PERSISTANCE_HPP
#define YAPPI_PERSISTANCE_HPP

#include "common.hpp"
#include "file_storage.hpp"
#include "eblob_storage.hpp"

namespace yappi { namespace persistance {

template<class Backend>
class storage_facade_t: public Backend {
    public:
        storage_facade_t(helpers::auto_uuid_t uuid):
            Backend(uuid)
        {}

    public:
        bool put(const std::string& key, const Json::Value& value) {
            return static_cast<Backend*>(this)->put(key, value);
        }

        bool exists(const std::string& key) const {
            return static_cast<const Backend*>(this)->exists(key);
        }

        Json::Value get(const std::string& key) const {
            return static_cast<const Backend*>(this)->get(key);
        }

        Json::Value all() const {
            return static_cast<const Backend*>(this)->all();
        }

        void remove(const std::string& key) {
            static_cast<Backend*>(this)->remove(key);
        }

        void purge() {
            static_cast<Backend*>(this)->purge();
        }
};

typedef storage_facade_t<backends::file_storage_t> storage_t;

}}

#endif
