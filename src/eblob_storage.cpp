#include <boost/tuple/tuple.hpp>
#include <boost/format.hpp>

#include "detail/eblobs.hpp"

using namespace yappi::helpers;
using namespace yappi::storage::backends;

namespace fs = boost::filesystem;

bool eblob_collector_t::callback(const zbr::eblob_disk_control* dco, const void* data, int) {
    if(dco->flags & BLOB_DISK_CTL_REMOVE) {
        return true;
    }

    std::string value(
        static_cast<const char*>(data),
        dco->data_size);

    Json::Value object;

    if(!m_reader.parse(value, object)) {
        syslog(LOG_ERR, "storage: malformed json - %s",
            m_reader.getFormatedErrorMessages().c_str());
        return true;
    } 
    
    m_root[auto_uuid_t().get()] = object;
    
    return true;
}

bool eblob_purger_t::callback(const zbr::eblob_disk_control* dco, const void* data, int) {
    m_keys.push_back(dco->key);
    return true;
}

void eblob_purger_t::complete(uint64_t, uint64_t) {
    for(key_list_t::const_iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
        m_eblob.remove_all(*it);
    }
}

eblob_storage_t::eblob_storage_t():
    m_storage_path(config_t::get().storage.path),
    m_logger(NULL, EBLOB_LOG_NOTICE)
{
    if(!fs::exists(m_storage_path)) {
        try {
            fs::create_directories(m_storage_path);
        } catch(const std::runtime_error& e) {
            throw std::runtime_error("cannot create " + m_storage_path.string());
        }
    } else if(fs::exists(m_storage_path) && !fs::is_directory(m_storage_path)) {
        throw std::runtime_error(m_storage_path.string() + " is not a directory");
    }
}

eblob_storage_t::~eblob_storage_t() {
    m_eblobs.clear();
}

void eblob_storage_t::put(const std::string& store, const std::string& key, const Json::Value& value) {
    if(config_t::get().storage.disabled)
        return;

    eblob_map_t::iterator it = m_eblobs.find(store);
    
    if(it == m_eblobs.end()) {
        zbr::eblob_config cfg;

        memset(&cfg, 0, sizeof(cfg));
        cfg.file = const_cast<char*>((m_storage_path / store).string().c_str());
        cfg.iterate_threads = 1;
        cfg.sync = 5;
        cfg.log = m_logger.log();

        boost::tie(it, boost::tuples::ignore) = m_eblobs.insert(store, new zbr::eblob(&cfg));
    }
        
    Json::FastWriter writer;
    std::string object = writer.write(value);    

    try {
        it->second->write_hashed(key, object, 0);
    } catch(const std::runtime_error& e) {
        throw std::runtime_error((boost::format("failed to write '%1%' to '%2%' - %3%")
            % key % store % e.what()).str());
    }
}

bool eblob_storage_t::exists(const std::string& store, const std::string& key) {
    if(config_t::get().storage.disabled)
        return false;

    eblob_map_t::iterator it = m_eblobs.find(store);
    
    if(it != m_eblobs.end()) {
        std::string object;

        try {
            object = it->second->read_hashed(key, 0, 0);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "storage: failed to read '%s' from '%s' - %s",
                key.c_str(), store.c_str(), e.what());
            return false;
        }

        return !object.empty();
    }

    return false;
}

Json::Value eblob_storage_t::get(const std::string& store, const std::string& key) {
    Json::Value root(Json::objectValue);
    
    if(config_t::get().storage.disabled)
        return root;

    eblob_map_t::iterator it = m_eblobs.find(store);
    
    if(it != m_eblobs.end()) {
        Json::Reader reader(Json::Features::strictMode());
        std::string object;

        try {
            object = it->second->read_hashed(key, 0, 0);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "storage: failed to read '%s' from '%s' - %s",
                key.c_str(), store.c_str(), e.what());
            return root;
        }

        if(!object.empty() && !reader.parse(object, root)) {
            syslog(LOG_ERR, "storage: malformed json for '%s' in '%s' - %s",
                key.c_str(), store.c_str(), reader.getFormatedErrorMessages().c_str());
        }
    }

    return root;
}

Json::Value eblob_storage_t::all(const std::string& store) const {
    if(config_t::get().storage.disabled)
        return Json::Value(Json::objectValue);

    eblob_collector_t collector;
    
    try {
        zbr::eblob_iterator iterator((m_storage_path / store).string(), true);
        iterator.iterate(collector, 1);
    } catch(...) {
        return Json::Value(Json::objectValue);
    }

    return collector.seal();
}

void eblob_storage_t::remove(const std::string& store, const std::string& key) {
    if(config_t::get().storage.disabled)
        return;

    eblob_map_t::iterator it = m_eblobs.find(store);
    
    if(it != m_eblobs.end()) {
        try {
            it->second->remove_hashed(key);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "storage: failed to remove '%s' from '%s' - %s",
                key.c_str(), store.c_str(), e.what());
        }
    }
}

void eblob_storage_t::purge(const std::string& store) {
    if(config_t::get().storage.disabled)
        return;

    eblob_map_t::iterator it = m_eblobs.find(store);

    if(it != m_eblobs.end()) {
        syslog(LOG_NOTICE, "storage: purging '%s'", store.c_str());
        
        eblob_purger_t purger(*it->second);
        
        try {
            zbr::eblob_iterator iterator((m_storage_path / store).string(), true);
            iterator.iterate(purger, 1);
        } catch(...) {
            // Nothing we can do about it
        }
    }
}
