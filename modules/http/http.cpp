//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <sstream>

#include <curl/curl.h>

#include "cocaine/plugin.hpp"

namespace cocaine { namespace plugin {

class http_t:
    public source_t
{
    public:
        static source_t* create(const std::string& args) {
            return new http_t(args);
        }

    public:
        http_t(const std::string& args) {
            m_curl = curl_easy_init();
            
            if(!m_curl) {
                throw unrecoverable_error_t("unable to initialize libcurl");
            }
            
            curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 1);
            curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 0);
            curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, &m_error_message);
            curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &nullwriter);
            curl_easy_setopt(m_curl, CURLOPT_URL, args.c_str());
            curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "Cocaine HTTP Plugin");
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
        
        virtual void invoke(
            callback_fn_t callback,
            const std::string& method,
            const void* request,
            size_t size) 
        {
            if(method != "check") {
                throw unrecoverable_error_t("unsupported method");
            }

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
        
            Json::FastWriter writer;
            std::string response(writer.write(result));

            callback(response.data(), response.size());
        }

    private:
        CURL* m_curl;
        char m_error_message[CURL_ERROR_SIZE];
};

static const source_info_t plugin_info[] = {
    { "http",  &http_t::create },
    { "https", &http_t::create },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        return plugin_info;
    }
}

}}
