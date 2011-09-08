#ifndef YAPPI_VOID_STORAGE_HPP
#define YAPPI_VOID_STORAGE_HPP

#include "common.hpp"
#include "storages/abstract.hpp"

namespace yappi { namespace storage { namespace backends {

class void_storage_t:
    public abstract_storage_t
{
    public:
        void put(const std::string& store, const std::string& key, const Json::Value& value) { }
        bool exists(const std::string& store, const std::string& key) { return false; }

        Json::Value get(const std::string& store, const std::string& key) { return Json::Value(); }
        Json::Value all(const std::string& store) { return Json::Value(); }

        void remove(const std::string& store, const std::string& key) { }
        void purge(const std::string& store) { }
};

}}}

#endif
