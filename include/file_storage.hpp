#ifndef YAPPI_FILE_STORAGE_HPP
#define YAPPI_FILE_STORAGE_HPP

#include <boost/filesystem.hpp>

#include "common.hpp"

namespace yappi { namespace persistance { namespace backends {

class file_storage_t {
    public:
        file_storage_t(const std::string& uuid);

    public:
        bool put(const std::string& key, const Json::Value& value);
        bool exists(const std::string& key) const;

        Json::Value get(const std::string& key) const;
        Json::Value all() const;

        void remove(const std::string& key);

    private:
        boost::filesystem::path m_storage_path;
};

}}}

#endif
