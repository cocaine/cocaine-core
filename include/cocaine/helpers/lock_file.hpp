#ifndef COCAINE_LOCK_FILE_HPP
#define COCAINE_LOCK_FILE_HPP

#include <errno.h>
#include <fcntl.h>

#include <boost/filesystem.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace helpers {
    
class lock_file_t:
    public boost::noncopyable
{
    public:
        lock_file_t(const boost::filesystem::path& filepath):
            m_filepath(filepath)
        {
            m_fd = open(m_filepath.string().c_str(), O_RDWR | O_NOATIME);

            if(m_fd < 0) {
                return;
            }

            if(lockf(m_fd, F_LOCK, 0) < 0) {
                throw std::runtime_error("failed to lock " + m_filepath.string());
            }
        }

        ~lock_file_t() {
            if(m_fd >= 0 && lockf(m_fd, F_ULOCK, 0) < 0) {
                throw std::runtime_error("failed to unlock " + m_filepath.string());
            } else {
                close(m_fd);
            }
        }
    
    private:
        const boost::filesystem::path m_filepath;
        int m_fd;
};

}}

#endif
