#include <sstream>

#include <curl/curl.h>

#include "cocaine/plugin.hpp"

namespace cocaine { namespace plugin {

class http_t:
    public source_t
{
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

        virtual ~http_t() {
            curl_easy_cleanup(m_curl);
        }
    
        static size_t nullwriter(void *ptr, size_t size, size_t nmemb, void *userdata) {
            return size * nmemb;
        }
        
        virtual uint32_t capabilities() const {
            return ITERATOR;
        }

        virtual Json::Value invoke() {
            Json::Value result;
        
            CURLcode code = curl_easy_perform(m_curl);
            
            if(code == 0) {
                long retcode = 0;
                
                curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &retcode);
                
                result["code"] = static_cast<Json::UInt>(retcode);
                result["availability"] = (retcode >= 200 && retcode < 300) ? "available" : "down";
            } else {
                result["code"] = 0;
                result["availability"] = "down";
                result["error"] = std::string(m_error_message);
            }
        
            return result;
        }

    private:
        CURL* m_curl;
        char m_error_message[CURL_ERROR_SIZE];
};

source_t* create_http_instance(const char* uri) {
    return new http_t(uri);
}

static const source_info_t plugin_info[] = {
    { "http", &create_http_instance },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        return plugin_info;
    }
}

}}
