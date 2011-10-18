#include <boost/filesystem/fstream.hpp>
#include <boost/iterator/filter_iterator.hpp>

#include "cocaine/storages/files.hpp"

using namespace cocaine::helpers;
using namespace cocaine::storage::backends;

namespace fs = boost::filesystem;

struct is_regular_file {
    template<typename T> bool operator()(T entry) {
        return fs::is_regular(entry);
    }
};

file_storage_t::file_storage_t():
    m_storage_path(config_t::get().storage.location),
    m_instance(config_t::get().core.instance)
{}

void file_storage_t::put(const std::string& ns, const std::string& key, const Json::Value& value) {
    boost::mutex::scoped_lock lock(m_mutex);
    fs::path store_path(m_storage_path / m_instance / ns);

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
    fs::path filepath(store_path / key);
    fs::ofstream stream(filepath, fs::ofstream::out | fs::ofstream::trunc);
   
    if(!stream) {
        throw std::runtime_error("failed to open " + filepath.string()); 
    }     

    Json::Value container;
    container["object"] = value;

    std::string json(writer.write(container));
    
    stream << json;
    stream.close();
}

bool file_storage_t::exists(const std::string& ns, const std::string& key) {
    boost::mutex::scoped_lock lock(m_mutex);
    fs::path filepath(m_storage_path / m_instance / ns / key);
    
    return fs::exists(filepath) && fs::is_regular(filepath);
}

Json::Value file_storage_t::get(const std::string& ns, const std::string& key) {
    boost::mutex::scoped_lock lock(m_mutex);
    Json::Value root(Json::objectValue);
    Json::Reader reader(Json::Features::strictMode());
    fs::path filepath(m_storage_path / m_instance / ns / key);
    fs::ifstream stream(filepath, fs::ifstream::in);
     
    if(stream) { 
        if(!reader.parse(stream, root)) {
            throw std::runtime_error("corrupted data in " + filepath.string());
        }
    }

    return root["object"];
}

Json::Value file_storage_t::all(const std::string& ns) {
    Json::Value root(Json::objectValue);
    fs::path store_path(m_storage_path / m_instance / ns);

    if(!fs::exists(store_path))
        return root;

    Json::Reader reader(Json::Features::strictMode());

    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    file_iterator it = file_iterator(is_regular_file(), fs::directory_iterator(store_path)), end;

    while(it != end) {
#if BOOST_FILESYSTEM_VERSION == 3
        Json::Value value(get(ns, it->path().filename().string()));
#else
        Json::Value value(get(ns, it->leaf()));
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

void file_storage_t::remove(const std::string& ns, const std::string& key) {
    boost::mutex::scoped_lock lock(m_mutex);
    fs::path file_path(m_storage_path / m_instance / ns / key);

    if(fs::exists(file_path)) {
        try {
            fs::remove(file_path);
        } catch(...) {
            throw std::runtime_error("failed to remove " + file_path.string());
        }
    }
}

void file_storage_t::purge(const std::string& ns) {
    boost::mutex::scoped_lock lock(m_mutex);
    fs::remove_all(m_storage_path / m_instance / ns);
}

