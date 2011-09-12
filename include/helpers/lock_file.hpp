#ifndef COCAINE_LOCK_FILE_HPP
#define COCAINE_LOCK_FILE_HPP

#include <errno.h>
#include <fcntl.h>

#include <boost/filesystem.hpp>

#include "common.hpp"

namespace cocaine { namespace helpers {
    
class lock_file_t:
    public boost::noncopyable
{
    public:
        lock_file_t(const boost::filesystem::path& filepath):
            m_filepath(filepath)
        {
            m_fd = open(m_filepath.string().c_str(), O_CREAT | O_EXCL | O_RDWR, 00600);

            if(m_fd < 0) {
                throw std::runtime_error("failed to create " + m_filepath.string());
            }

            if(lockf(m_fd, F_TLOCK, 0) < 0) {
                throw std::runtime_error("failed to lock " + m_filepath.string());
            }
        }

        ~lock_file_t() {
            lockf(m_fd, F_ULOCK, 0);
            close(m_fd);
            boost::filesystem::remove(m_filepath);
        }
    
    private:
        boost::filesystem::path m_filepath;
        int m_fd;
};

}}

#endif
