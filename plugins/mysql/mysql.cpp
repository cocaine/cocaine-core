#include <string>
#include <sstream>

#include <mysql.h>

#include "uri.hpp"
#include "plugin.hpp"

namespace yappi { namespace plugin {

class mysql_t: public source_t {
    public:
        mysql_t(const std::string& uri_):
            source_t(uri_),
            m_connect_timeout(1),
            m_read_timeout(1),
            m_write_timeout(1)
        {
            // uri: mysql://user:pass@host.yandex.net:3306/db
            yappi::helpers::uri_t uri(uri_);
    
            m_host = uri.host();
            m_port = uri.port();
            m_db = uri.path().back();

            m_username = uri.userinfo().substr(0, uri.userinfo().find_first_of(":"));
            m_password = uri.userinfo().substr(uri.userinfo().find_first_of(":") + 1);
        }

        dict_t fetch() {
            dict_t dict;
            MYSQL* connection = mysql_init(NULL);

            if(!connection) {
                dict["availability"] = "down";
                return dict;
            }

            mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, reinterpret_cast<const char*>(&m_connect_timeout));
            mysql_options(connection, MYSQL_OPT_READ_TIMEOUT, reinterpret_cast<const char*>(&m_read_timeout));
            mysql_options(connection, MYSQL_OPT_WRITE_TIMEOUT, reinterpret_cast<const char*>(&m_write_timeout));
            MYSQL* result = mysql_real_connect(connection, m_host.c_str(), m_username.c_str(), m_password.c_str(), m_db.c_str(), m_port, NULL, 0);
            
            dict["availability"] = result ? "available" : "down";

            mysql_close(connection);
            return dict;
        }

    private:
        std::string m_host, m_username, m_password, m_db;
        unsigned int m_port, m_connect_timeout, m_read_timeout, m_write_timeout;
};

void* create_mysql_instance(const char* uri) {
    return new mysql_t(uri);
}

const plugin_info_t plugin_info = {
    1,
    {
        { "mysql", &create_mysql_instance }
    }
};

extern "C" {
    const plugin_info_t* initialize() {
        return &plugin_info;
    }
}

}}
