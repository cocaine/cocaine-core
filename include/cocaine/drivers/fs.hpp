#ifndef COCAINE_DRIVERS_FS_HPP
#define COCAINE_DRIVERS_FS_HPP

#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

class fs_t:
    public driver_t
{
    public:
        fs_t(const std::string& method, 
             boost::shared_ptr<engine_t> parent,
             const Json::Value& args);

    public:
        void operator()(ev::stat&, int);
        virtual Json::Value info() const;

    private:
        const std::string m_path;
        ev::stat m_watcher;
};

}}}

#endif
