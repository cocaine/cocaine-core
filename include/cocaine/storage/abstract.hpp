#ifndef COCAINE_ABSTRACT_STORAGE
#define COCAINE_ABSTRACT_STORAGE

#include "cocaine/common.hpp"

namespace cocaine { namespace storage {

class abstract_storage_t:
    public boost::noncopyable
{
    public:
        virtual void put(const std::string& store, const std::string& key, const Json::Value& value) = 0;
        virtual bool exists(const std::string& store, const std::string& key) = 0;

        virtual Json::Value get(const std::string& store, const std::string& key) = 0;
        virtual Json::Value all(const std::string& store) = 0;

        virtual void remove(const std::string& store, const std::string& key) = 0;
        virtual void purge(const std::string& store) = 0;
};

}}

#endif
