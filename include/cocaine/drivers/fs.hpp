#ifndef COCAINE_DRIVERS_FS_HPP
#define COCAINE_DRIVERS_FS_HPP

#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

class fs_t:
    public driver_t
{
    public:
        fs_t(engine_t* engine,
             const std::string& method, 
             const Json::Value& args);
        virtual ~fs_t();

        virtual Json::Value info() const;

        void operator()(ev::stat&, int);

    private:
        const std::string m_path;
        ev::stat m_watcher;
};

}}}

#endif
