#ifndef YAPPI_FILE_STORAGE_HPP
#define YAPPI_FILE_STORAGE_HPP

#include <boost/filesystem.hpp>

#include "common.hpp"
#include "detail/abstract.hpp"

namespace yappi { namespace storage { namespace backends {

class file_storage_t:
    public abstract_storage_t
{
    public:
        file_storage_t();

    public:
        virtual void put(const std::string& store, const std::string& key, const Json::Value& value);
        virtual bool exists(const std::string& store, const std::string& key);

        virtual Json::Value get(const std::string& store, const std::string& key);
        virtual Json::Value all(const std::string& store);

        virtual void remove(const std::string& store, const std::string& key);
        virtual void purge(const std::string& store);

    private:
        boost::filesystem::path m_storage_path;
        std::string m_instance;
};

}}}

#endif
