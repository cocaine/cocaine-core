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

#include <boost/filesystem/fstream.hpp>

#include <curl/curl.h>

#include "cocaine/downloads.hpp"

using namespace cocaine::helpers;

local_t::local_t(const uri_t& uri) {
    std::stringstream stream;
    std::vector<std::string> target(uri.path());

    for(std::vector<std::string>::const_iterator it = target.begin(); it != target.end(); ++it) {
        m_path /= *it;
    }
   
    boost::filesystem::ifstream input(m_path);
    
    if(!input) {
        throw std::runtime_error("unable to open " + m_path.string());
    }

    stream << input.rdbuf();
    
    m_blob = stream.str();

#if BOOST_FILESYSTEM_VERSION == 3
    m_path = m_path.parent_path();
#else
    m_path = m_path.branch_path();
#endif
}

namespace {
    size_t stream_writer(void* data, size_t size, size_t nmemb, void* stream) {
        std::stringstream* out(reinterpret_cast<std::stringstream*>(stream));
        out->write(reinterpret_cast<char*>(data), size * nmemb);
        return size * nmemb;
    }
}

http_t::http_t(const helpers::uri_t& uri) {
    std::stringstream stream;

    char error_message[CURL_ERROR_SIZE];
    CURL* curl = curl_easy_init();

    if(!curl) {
        throw std::runtime_error("unable to initialize libcurl");
    }

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_message);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &stream_writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_URL, uri.source().c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Cocaine/0.6");
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
