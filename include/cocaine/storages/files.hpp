#ifndef COCAINE_FILE_STORAGE_HPP
#define COCAINE_FILE_STORAGE_HPP

#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/storages/abstract.hpp"

namespace cocaine { namespace storage { namespace backends {

class file_storage_t:
    public abstract_storage_t
{
    public:
        file_storage_t();

    public:
        virtual void put(const std::string& ns, const std::string& key, const Json::Value& value);
        virtual bool exists(const std::string& ns, const std::string& key);

        virtual Json::Value get(const std::string& ns, const std::string& key);
        virtual Json::Value all(const std::string& ns);

        virtual void remove(const std::string& ns, const std::string& key);
        virtual void purge(const std::string& ns);

    private:
        boost::mutex m_mutex;
        const boost::filesystem::path m_storage_path;
        const std::string m_instance;
};

}}}

#endif
