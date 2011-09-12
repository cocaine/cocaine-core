#ifndef YAPPI_PID_FILE_HPP
#define YAPPI_PID_FILE_HPP

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "common.hpp"

namespace yappi { namespace helpers {

class pid_file_t:
    public boost::noncopyable
{
    public:
        pid_file_t(const boost::filesystem::path& filepath):
            m_filepath(filepath)
        {
            boost::filesystem::ofstream stream(m_filepath,
                boost::filesystem::ofstream::out | boost::filesystem::ofstream::trunc);

            if(!stream) {
                throw std::runtime_error("failed to access " + m_filepath.string());
            }

            stream << getpid();
            stream.close();
        }

        ~pid_file_t() {
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
