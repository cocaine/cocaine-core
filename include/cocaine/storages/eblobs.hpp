#ifndef COCAINE_EBLOB_STORAGE_HPP
#define COCAINE_EBLOB_STORAGE_HPP

#include <boost/filesystem.hpp>

#include <eblob/eblob.hpp>

#include "cocaine/storages/abstract.hpp"

namespace cocaine { namespace storage { namespace backends {

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
    public abstract_storage_t
{
    public:
        eblob_storage_t();
        ~eblob_storage_t();

    public:
        virtual void put(const std::string& ns, const std::string& key, const Json::Value& value);
        virtual bool exists(const std::string& ns, const std::string& key);

        virtual Json::Value get(const std::string& ns, const std::string& key);
        virtual Json::Value all(const std::string& ns) const;

        virtual void remove(const std::string& ns, const std::string& key);
        virtual void purge(const std::string& ns);

    private:
        const boost::filesystem::path m_storage_path;

        typedef boost::ptr_map<const std::string, zbr::eblob> eblob_map_t;
        eblob_map_t m_eblobs;

        zbr::eblob_logger m_logger;
};

}}}

#endif
