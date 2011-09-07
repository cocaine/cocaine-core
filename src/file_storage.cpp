#include <boost/filesystem/fstream.hpp>
#include <boost/iterator/filter_iterator.hpp>

#include "detail/files.hpp"

using namespace yappi::helpers;
using namespace yappi::storage::backends;

namespace fs = boost::filesystem;

struct is_regular_file {
    template<typename T> bool operator()(T entry) {
        return fs::is_regular(entry);
    }
};

file_storage_t::file_storage_t():
    m_storage_path(config_t::get().storage.path)
{}

void file_storage_t::put(const std::string& store, const std::string& key, const Json::Value& value) {
    if(config_t::get().storage.disabled)
        return;

    fs::path store_path = m_storage_path / store;

    if(!fs::exists(store_path)) {
        try {
            fs::create_directories(store_path);
        } catch(const std::runtime_error& e) {
            throw std::runtime_error("cannot create " + store_path.string());
        }
    } else if(fs::exists(store_path) && !fs::is_directory(store_path)) {
        throw std::runtime_error(store_path.string() + " is not a directory");
    }

    Json::StyledWriter writer;
    fs::path filepath = store_path / key;
    fs::ofstream stream(filepath, fs::ofstream::out | fs::ofstream::trunc);
   
    if(!stream) {
        throw std::runtime_error("failed to write " + filepath.string());
    }     

    std::string json = writer.write(value);
    
    stream << json;
    stream.close();
}

bool file_storage_t::exists(const std::string& store, const std::string& key) const {
    if(config_t::get().storage.disabled)
        return false;

    fs::path filepath = m_storage_path / store / key;
    return fs::exists(filepath) && fs::is_regular(filepath);
}

Json::Value file_storage_t::get(const std::string& store, const std::string& key) const {
    Json::Value root(Json::objectValue);
    
    if(config_t::get().storage.disabled)
        return root;

    Json::Reader reader(Json::Features::strictMode());
    fs::path filepath = m_storage_path / store / key;
    fs::ifstream stream(filepath, fs::ifstream::in);
     
    if(stream) { 
        if(!reader.parse(stream, root)) {
            syslog(LOG_WARNING, "storage: malformed json in %s - %s",
                filepath.string().c_str(), reader.getFormatedErrorMessages().c_str());
        }
    }

    return root;
}

Json::Value file_storage_t::all(const std::string& store) const {
    Json::Value root(Json::objectValue);
    fs::path store_path = m_storage_path / store;

    if(config_t::get().storage.disabled || !fs::exists(store_path))
        return root;

    Json::Reader reader(Json::Features::strictMode());

    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    file_iterator it = file_iterator(is_regular_file(), fs::directory_iterator(store_path)), end;

    while(it != end) {
#if BOOST_FILESYSTEM_VERSION == 3
        Json::Value value = get(store, it->path().filename().string());
#else
        Json::Value value = get(store, it->leaf());
#endif

        if(!value.empty()) {
#if BOOST_FILESYSTEM_VERSION == 3
            root[it->path().filename().string()] = value;
#else
            root[it->leaf()] = value;
#endif
        }

        ++it;
    }

    return root;
}

void file_storage_t::remove(const std::string& store, const std::string& key) {
    if(config_t::get().storage.disabled)
        return;

    fs::remove(m_storage_path / store / key);
}

void file_storage_t::purge(const std::string& store) {
    if(config_t::get().storage.disabled)
        return;

    syslog(LOG_NOTICE, "storage: purging");
    fs::remove_all(m_storage_path / store);
}

