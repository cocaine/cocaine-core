#include <boost/filesystem/fstream.hpp>

#include <curl/curl.h>

#include "cocaine/downloads.hpp"
#include "cocaine/security/digest.hpp"

using namespace cocaine::helpers;
using namespace cocaine::security;

namespace fs = boost::filesystem;

cache_t::cache_t(const uri_t& uri) {
    std::stringstream stream;
    std::vector<std::string> target(uri.path());

    // Cut off the URI scheme and hash all the remainings    
    std::string identity(digest_t().get(
        uri.source().substr(
            uri.source().find_first_of(":") + 3)
        ));

    m_path = fs::path(config_t::get().downloads.location) / identity;
    fs::path location(m_path);

    for(std::vector<std::string>::const_iterator it = target.begin(); it != target.end(); ++it) {
        location /= *it;
    }
   
    fs::ifstream input(location);
    
    if(!input) {
        throw std::runtime_error("failed to open " + location.string());
    }

    stream << input.rdbuf();
    m_blob = stream.str();
}

size_t stream_writer(void* data, size_t size, size_t nmemb, void* stream) {
    std::stringstream* out(reinterpret_cast<std::stringstream*>(stream));
    out->write(reinterpret_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

http_t::http_t(const helpers::uri_t& uri) {
    try {
        // Try to fetch it from the cache first
        m_blob = cache_t(uri);
    } catch(...) {
        syslog(LOG_DEBUG, "downloads: cache is empty, fetching from '%s'", uri.host().c_str());
    }

    std::stringstream stream;

    char error_message[CURL_ERROR_SIZE];
    CURL* curl = curl_easy_init();

    if(!curl) {
        throw std::runtime_error("failed to initialize libcurl");
    }

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_message);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &stream_writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_URL, uri.source().c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Cocaine/0.5");
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1000);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

    if(curl_easy_perform(curl) != 0) {
        throw std::runtime_error(error_message);
    }

    curl_easy_cleanup(curl);

    m_blob = stream.str();
}
