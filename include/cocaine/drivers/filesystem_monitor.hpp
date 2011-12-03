#ifndef COCAINE_DRIVER_FILESYSTEM_MONITOR_HPP
#define COCAINE_DRIVER_FILESYSTEM_MONITOR_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace driver {

class filesystem_monitor_t:
    public driver_t
{
    public:
        filesystem_monitor_t(engine_t* engine,
                             const std::string& method, 
                             const Json::Value& args);
        virtual ~filesystem_monitor_t();

        virtual Json::Value info() const;

    private:
        void event(ev::stat&, int);

    private:
        const std::string m_path;
        ev::stat m_watcher;
};

}}}

#endif
