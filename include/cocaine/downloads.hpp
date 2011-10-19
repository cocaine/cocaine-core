#ifndef COCAINE_DOWNLOADS
#define COCAINE_DOWNLOADS

#include "cocaine/common.hpp"
#include "cocaine/helpers/uri.hpp"

namespace cocaine { namespace helpers {

class download_t {
    public:
        inline operator std::string() {
            return m_blob;
        }

    protected:
        download_t() { }

    protected:
        std::string m_blob;
};

class cache_t:
    public download_t
{
    public:
        cache_t(const uri_t& uri);
};

class http_t:
    public download_t
{
    public:
        http_t(const uri_t& uri);
};

download_t download(const uri_t& uri) {
    if(uri.scheme() == "cache") {
        return cache_t(uri);
    } else if(uri.scheme() == "http" || uri.scheme() == "https") {
        return http_t(uri);
    } else {
        throw std::runtime_error("unsupported protocol - '" + uri.scheme() + "'");
    }
}

}}

#endif
