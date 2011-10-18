#ifndef COCAINE_VOID_STORAGE_HPP
#define COCAINE_VOID_STORAGE_HPP

#include "cocaine/storages/abstract.hpp"

namespace cocaine { namespace storage { namespace backends {

class void_storage_t:
    public abstract_storage_t
{
    public:
        void put(const std::string& ns, const std::string& key, const Json::Value& value) { }
        bool exists(const std::string& ns, const std::string& key) { return false; }

        Json::Value get(const std::string& ns, const std::string& key) { return Json::Value(); }
        Json::Value all(const std::string& ns) { return Json::Value(); }

        void remove(const std::string& ns, const std::string& key) { }
        void purge(const std::string& ns) { }
};

}}}

#endif
