#ifndef COCAINE_PID_FILE_HPP
#define COCAINE_PID_FILE_HPP

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace helpers {

class pid_file_t:
    public boost::noncopyable
{
    public:
        pid_file_t(const boost::filesystem::path& filepath):
            m_filepath(filepath)
        {
            // If the pidfile exists, check if the process is still active
            if(boost::filesystem::exists(m_filepath)) {
                pid_t pid;
                boost::filesystem::ifstream stream(m_filepath);

                if(stream) {
                    stream >> pid;

                    if(kill(pid, 0) < 0 && errno == ESRCH) {
                        // Stale pid file
                        remove();
                    } else {
                        throw std::runtime_error("another instance is active");
                    }
                } else {
                    throw std::runtime_error("failed to read " + m_filepath.string());
                }
            }

            boost::filesystem::ofstream stream(m_filepath);

            if(!stream) {
                throw std::runtime_error("failed to write " + m_filepath.string());
            }

            stream << getpid();
            stream.close();
        }

        ~pid_file_t() {
            remove();
        }

    private:
        void remove() {
            try {
                boost::filesystem::remove(m_filepath);
            } catch(const std::runtime_error& e) {
                throw std::runtime_error("failed to remove " + m_filepath.string());
            }
        }

    private:
        boost::filesystem::path m_filepath;
};

}}

#endif
