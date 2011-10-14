#ifndef COCAINE_DRIVERS_FS_HPP
#define COCAINE_DRIVERS_FS_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace drivers {

class fs_t:
    public driver_base_t<ev::stat, fs_t>
{
    public:
        fs_t(const std::string& name, boost::shared_ptr<engine_t> parent, const Json::Value& args):
            driver_base_t<ev::stat, fs_t>(name, parent),
            m_path(args.get("path", "").asString())
        {
            if(m_path.empty()) {
                throw std::runtime_error("no path specified");
            }

            m_id = "fs:" + digest_t().get(m_name + m_path);
        }

        inline void initialize() {
            m_watcher->set(m_path.c_str());
        }

    private:
        const std::string m_path;
};

}}}

#endif
