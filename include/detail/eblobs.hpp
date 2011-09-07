#ifndef YAPPI_EBLOB_STORAGE_HPP
#define YAPPI_EBLOB_STORAGE_HPP

#include <boost/filesystem.hpp>
#include <boost/thread/tss.hpp>

#include <eblob/eblob.hpp>

#include "common.hpp"

namespace yappi { namespace storage { namespace backends {

class eblob_collector_t:
    public zbr::eblob_iterator_callback
{
    public:
        bool callback(const zbr::eblob_disk_control* dco, const void* data, int);
        void complete(uint64_t, uint64_t) {}

        inline Json::Value seal() { return m_root; }

    private:
        Json::Reader m_reader;
        Json::Value m_root;
};

class eblob_purger_t:
    public zbr::eblob_iterator_callback
{
    public:
        eblob_purger_t(zbr::eblob* eblob):
            m_eblob(eblob)
        {}

        bool callback(const zbr::eblob_disk_control* dco, const void* data, int);
        void complete(uint64_t, uint64_t);

    private:
        zbr::eblob* m_eblob;

        typedef std::vector<zbr::eblob_key> key_list_t;
        key_list_t m_keys;
};

class eblob_storage_t:
    public boost::noncopyable,
    public helpers::factory_t<eblob_storage_t, boost::thread_specific_ptr>
{
    public:
        eblob_storage_t();
        ~eblob_storage_t();

    public:
        void put(const std::string& store, const std::string& key, const Json::Value& value);
        bool exists(const std::string& store, const std::string& key);

        Json::Value get(const std::string& store, const std::string& key);
        Json::Value all(const std::string& store) const;

        void remove(const std::string& store, const std::string& key);
        void purge(const std::string& store);

    private:
        boost::filesystem::path m_storage_path;

        typedef boost::ptr_map<const std::string, zbr::eblob> eblob_map_t;
        eblob_map_t m_eblobs;

        zbr::eblob_logger m_logger;
};

}}}

#endif
