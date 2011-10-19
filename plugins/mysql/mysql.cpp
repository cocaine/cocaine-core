#include <mysql.h>

#include "cocaine/plugin.hpp"
#include "cocaine/helpers/uri.hpp"

namespace cocaine { namespace plugin {

class mysql_t:
    public source_t
{
    public:
        mysql_t(const std::string& name, const std::string& args):
            source_t(name),
            m_connect_timeout(1),
            m_read_timeout(1),
            m_write_timeout(1)
        {
            // uri: mysql://user:pass@host.yandex.net:3306/db
            cocaine::helpers::uri_t uri(args);
    
            m_host = uri.host();
            m_port = uri.port();
            m_db = uri.path().back();

            m_username = uri.userinfo().substr(0, uri.userinfo().find_first_of(":"));
            m_password = uri.userinfo().substr(uri.userinfo().find_first_of(":") + 1);
        }

        virtual Json::Value invoke(const std::string& callable, const void* request = NULL, size_t request_length = 0) {
            Json::Value result;
            MYSQL* connection = mysql_init(NULL);

            if(connection) {
                mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, reinterpret_cast<const char*>(&m_connect_timeout));
                mysql_options(connection, MYSQL_OPT_READ_TIMEOUT, reinterpret_cast<const char*>(&m_read_timeout));
                mysql_options(connection, MYSQL_OPT_WRITE_TIMEOUT, reinterpret_cast<const char*>(&m_write_timeout));
                MYSQL* status = mysql_real_connect(connection, m_host.c_str(), m_username.c_str(), m_password.c_str(), m_db.c_str(), m_port, NULL, 0);
                result["availability"] = status ? "available" : "down";
            } else {
                result["availability"] = "down";
            }

            mysql_close(connection);

            return result;
        }

    private:
        std::string m_host, m_username, m_password, m_db;
        unsigned int m_port, m_connect_timeout, m_read_timeout, m_write_timeout;
};

source_t* create_mysql_instance(const char* name, const char* args) {
    return new mysql_t(name, args);
}

static const source_info_t plugin_info[] = {
    { "mysql", &create_mysql_instance },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        return plugin_info;
    }
}

}}
