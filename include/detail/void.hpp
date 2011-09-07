#ifndef YAPPI_VOID_STORAGE_HPP
#define YAPPI_VOID_STORAGE_HPP

#include <boost/thread/tss.hpp>

#include "common.hpp"

namespace yappi { namespace storage { namespace backends {

class void_storage_t:
    public boost::noncopyable,
    public helpers::factory_t<void_storage_t, boost::thread_specific_ptr>
{
    public:
        void put(const std::string& store, const std::string& key, const Json::Value& value) { }
        bool exists(const std::string& store, const std::string& key) const { return false; }

        Json::Value get(const std::string& store, const std::string& key) const { return Json::Value(); }
        Json::Value all(const std::string& store) const { return Json::Value(); }

        void remove(const std::string& store, const std::string& key) { }
        void purge(const std::string& store) { }
};

}}}

#endif
