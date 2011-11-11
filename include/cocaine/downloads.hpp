#ifndef COCAINE_DOWNLOADS
#define COCAINE_DOWNLOADS

#include <boost/filesystem.hpp>

#include "cocaine/common.hpp"
#include "cocaine/helpers/uri.hpp"

namespace cocaine { namespace helpers {

class download_t {
    public:
        inline operator std::string() {
            return m_blob;
        }

        inline boost::filesystem::path path() const {
            return m_path;
        }
    
    protected:
        download_t():
            m_path("/")
        { }

    protected:
        std::string m_blob;
        boost::filesystem::path m_path;
};

class local_t:
    public download_t
{
    public:
        local_t(const uri_t& uri);
};

class http_t:
    public download_t
{
    public:
        http_t(const uri_t& uri);
};

download_t download(const uri_t& uri) {
    if(uri.scheme() == "local") {
        return local_t(uri);
    } else if(uri.scheme() == "http" || uri.scheme() == "https") {
        return http_t(uri);
    } else {
        throw std::runtime_error("unsupported protocol - '" + uri.scheme() + "'");
    }
}

}}

#endif
