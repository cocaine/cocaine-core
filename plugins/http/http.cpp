#include <stdexcept>
#include <sstream>

#include <curl/curl.h>

#include "plugin.hpp"

namespace yappi { namespace plugin {

class http_t: public source_t {
    public:
        http_t(const std::string& uri):
            source_t(uri)
        {
            m_curl = curl_easy_init();
            
            if(!m_curl) {
                throw std::runtime_error("failed to initialize libcurl");
            }
            
            curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 1);
            curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 0);
            curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, &m_error_message);
            curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &nullwriter);
            curl_easy_setopt(m_curl, CURLOPT_URL, uri.c_str());
            curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "Yappi HTTP Plugin");
            curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1);
            curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
            curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, 500);
            curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
            curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1);
        }

        ~http_t() {
            curl_easy_cleanup(m_curl);
        }
    
        static size_t nullwriter(void *ptr, size_t size, size_t nmemb, void *userdata) {
            return size * nmemb;
        }
        
        virtual dict_t fetch() {
            dict_t dict;
        
            CURLcode result = curl_easy_perform(m_curl);
            
            if (result == 0) {
                long retcode = 0;
                curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &retcode);
            
                std::ostringstream fmt;
                fmt << retcode;

                dict["code"] = fmt.str();
                dict["availability"] = (retcode >= 200 && retcode < 300) ? "available" : "down";
            } else {
                dict["code"] = "0";
                dict["avalability"] = "down";
                dict["exception"] = std::string(m_error_message);
            }
        
            return dict;
        }

    private:
        CURL* m_curl;
        char m_error_message[CURL_ERROR_SIZE];
};

source_t* create_http_instance(const char* uri) {
    return new http_t(uri);
}

static const plugin_info_t plugin_info = {
    1,
    {
        { "http", &create_http_instance }
    }
};

extern "C" {
    const plugin_info_t* initialize() {
        return &plugin_info;
    }
}

}}
