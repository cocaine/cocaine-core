#ifndef COCAINE_STORAGES_ABSTRACT_HPP
#define COCAINE_STORAGES_ABSTRACT_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace storage {

class storage_t:
    public boost::noncopyable
{
    public:
        static boost::shared_ptr<storage_t> create();
    
    public:
        virtual void put(const std::string& ns, const std::string& key, const Json::Value& value) = 0;
        virtual bool exists(const std::string& ns, const std::string& key) = 0;

        virtual Json::Value get(const std::string& ns, const std::string& key) = 0;
        virtual Json::Value all(const std::string& ns) = 0;

        virtual void remove(const std::string& ns, const std::string& key) = 0;
        virtual void purge(const std::string& ns) = 0;
};

}}

#endif
