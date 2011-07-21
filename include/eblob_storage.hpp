#ifndef YAPPI_EBLOB_STORAGE_HPP
#define YAPPI_EBLOB_STORAGE_HPP

#include <boost/filesystem.hpp>
#include <eblob/eblob.hpp>

#include "common.hpp"

namespace yappi { namespace persistance { namespace backends {

class eblob_collector_t: public zbr::eblob_iterator_callback {
    public:
        bool callback(const zbr::eblob_disk_control* dco, const void* data, int);
        void complete(uint64_t, uint64_t) {}

        inline Json::Value seal() { return m_root; }

    private:
        Json::Reader m_reader;
        Json::Value m_root;
};

class eblob_purger_t: public zbr::eblob_iterator_callback {
    public:
        typedef std::vector<zbr::eblob_key> key_list_t;
        
        bool callback(const zbr::eblob_disk_control* dco, const void* data, int);
        void complete(uint64_t, uint64_t) {}

        inline key_list_t keys() { return m_keys; }

    private:
        key_list_t m_keys;
};

class eblob_storage_t {
    public:
        eblob_storage_t(const std::string& uuid, bool purge);
        virtual ~eblob_storage_t();

    public:
        bool put(const std::string& key, const Json::Value& value);
        bool exists(const std::string& key) const;

        Json::Value get(const std::string& key) const;
        Json::Value all() const;

        void remove(const std::string& key);

    private:
        boost::filesystem::path m_storage_path;
        std::auto_ptr<zbr::eblob> m_eblob;
        // zbr::eblob_logger m_logger;
};

}}}

#endif
