#ifndef COCAINE_STORAGE_FILES_HPP
#define COCAINE_STORAGE_FILES_HPP

#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/storages/base.hpp"

namespace cocaine { namespace storage {

class file_storage_t:
    public storage_t
{
    public:
        file_storage_t();

        virtual void put(const std::string& ns, const std::string& key, const Json::Value& value);

        virtual bool exists(const std::string& ns, const std::string& key);
        virtual Json::Value get(const std::string& ns, const std::string& key);
        virtual Json::Value all(const std::string& ns);

        virtual void remove(const std::string& ns, const std::string& key);
        virtual void purge(const std::string& ns);

    private:
        const boost::filesystem::path m_storage_path;
        const std::string m_instance;
};

}}

#endif
