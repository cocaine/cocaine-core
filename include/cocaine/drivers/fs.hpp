#ifndef COCAINE_DRIVER_FS_HPP
#define COCAINE_DRIVER_FS_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace driver {

class fs_t:
    public driver_t
{
    public:
        fs_t(engine_t* engine,
             const std::string& method, 
             const Json::Value& args);

        virtual Json::Value info() const;

    private:
        void event(ev::stat&, int);

    private:
        const std::string m_path;
        ev::stat m_watcher;
};

}}}

#endif
