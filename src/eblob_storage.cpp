#include "eblob_storage.hpp"

using namespace yappi::helpers;
using namespace yappi::persistance::backends;

namespace fs = boost::filesystem;

bool eblob_collector_t::callback(const zbr::eblob_disk_control* dco, const void* data, int flags) {
    if(dco->flags & BLOB_DISK_CTL_REMOVE) {
        return true;
    }

    std::string value(
        static_cast<const char*>(data),
        dco->data_size);

    Json::Value object;

    if(!m_reader.parse(value, object)) {
        syslog(LOG_ERR, "storage: malformed object in eblob: %s",
            m_reader.getFormatedErrorMessages().c_str());
        return true;
    }

    m_root[auto_uuid_t().get()] = object;
    return true;
}

eblob_storage_t::eblob_storage_t(const std::string& uuid):
    m_storage_path("/var/lib/yappi/" + uuid),
    m_eblob_log_path("/var/log/yappi-storage.log"),
    m_eblob_log_flags(EBLOB_LOG_NOTICE)
{
    if(!fs::exists(m_storage_path.branch_path())) {
       try {
           syslog(LOG_INFO, "storage: creating storage directory %s",
               m_storage_path.branch_path().string().c_str());
           fs::create_directories(m_storage_path.branch_path());
       } catch(const std::runtime_error& e) {
           throw std::runtime_error("cannot create " + m_storage_path.branch_path().string());
       }
    }

    syslog(LOG_INFO, "storage: initializing eblobs");
    m_eblob.reset(new zbr::eblob(const_cast<char*>(m_eblob_log_path.c_str()),
        m_eblob_log_flags, m_storage_path.string()));
}

eblob_storage_t::~eblob_storage_t() {
    m_eblob.reset(NULL);
}

bool eblob_storage_t::put(const std::string& key, const Json::Value& value) {
    Json::FastWriter writer;
    std::string object = writer.write(value);    

    try {
        m_eblob->write_hashed(key, object, BLOB_DISK_CTL_NOCSUM);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "storage: failed to write - %s", e.what());
        return false;
    }
        
    return true;
}

bool eblob_storage_t::exists(const std::string& key) const {
    std::string object;

    try {
        object = m_eblob->read_hashed(key, 0, 0);
    } catch(...) {
        return false;
    }

    return !object.empty();
}

Json::Value eblob_storage_t::get(const std::string& key) const {
    Json::Reader reader(Json::Features::strictMode());
    Json::Value root(Json::objectValue);
    std::string object;

    try {
        object = m_eblob->read_hashed(key, 0, 0);
    } catch(...) {
        return root;
    }

    if(!object.empty() && !reader.parse(object, root)) {
        syslog(LOG_ERR, "storage: malformed object in eblob - %s",
            reader.getFormatedErrorMessages().c_str());
    }

    return root;
}

Json::Value eblob_storage_t::all() const {
    eblob_collector_t collector;
    
    try {
        zbr::eblob_iterator iterator(m_storage_path.string(), true);
        iterator.iterate(collector, 1);
    } catch(...) {
        syslog(LOG_DEBUG, "storage: storage is empty");
    }

    return collector.seal();
}

void eblob_storage_t::remove(const std::string& key) {
    try {
        m_eblob->remove_hashed(key);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "storage: failed to remove - %s", e.what());
    }
}
