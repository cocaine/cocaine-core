#include <stdexcept>

#include <stdio.h>
#include <sys/sysinfo.h>

#include "plugin.hpp"

using namespace yappi::plugins;

class sysinfo_t: public source_t {
    public:
        sysinfo_t(const std::string& uri) {
            if(uri != "sysinfo://localhost") {
                throw std::invalid_argument("only localhost allowed");
            }
        }
    
        virtual dict_t fetch() {
            dict_t dict;
            char buf[32];
            struct sysinfo si;
            
            sysinfo(&si);

            sprintf(buf, "%lu %lu %lu", si.loads[0], si.loads[1], si.loads[2]);
            dict["loads"] = buf;

            sprintf(buf, "%lu", si.uptime);
            dict["uptime"] = buf;

            sprintf(buf, "%lu", si.freeram);
            dict["freeram"] = buf;

            return dict;
        }
};

void* create_instance(const char* uri) {
    return new sysinfo_t(uri);
}

static const plugin_info_t plugin_info = {
    1, 
    {
        { "sysinfo", &create_instance }
    }
};

extern "C" {
    const plugin_info_t* initialize() {
        return &plugin_info;
    }
}
