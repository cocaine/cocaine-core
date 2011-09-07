#ifndef YAPPI_FILE_STORAGE_HPP
#define YAPPI_FILE_STORAGE_HPP

#include <boost/filesystem.hpp>
#include <boost/thread/tss.hpp>

#include "common.hpp"

namespace yappi { namespace storage { namespace backends {

class file_storage_t:
    public boost::noncopyable,
    public helpers::factory_t<file_storage_t, boost::thread_specific_ptr>
{
    public:
        file_storage_t();

    public:
        void put(const std::string& store, const std::string& key, const Json::Value& value);
        bool exists(const std::string& store, const std::string& key) const;

        Json::Value get(const std::string& store, const std::string& key) const;
        Json::Value all(const std::string& store) const;

        void remove(const std::string& store, const std::string& key);
        void purge(const std::string& store);

    private:
        boost::filesystem::path m_storage_path;
        std::string m_instance;
};

}}}

#endif
