#ifndef YAPPI_PERSISTANCE_HPP
#define YAPPI_PERSISTANCE_HPP

#include <boost/filesystem.hpp>

#include "common.hpp"

namespace yappi { namespace persistance {

class file_storage_t {
    public:
        file_storage_t(const std::string& storage_path);

    public:
        bool put(const std::string& key, const Json::Value& value);
        
        Json::Value get(const std::string& key);
        Json::Value all();

        void remove(const std::string& key);

    private:
        boost::filesystem::path m_storage_path;
};

}}

#endif
