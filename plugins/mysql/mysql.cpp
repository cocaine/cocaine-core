#include <mysql.h>

#include "cocaine/plugin.hpp"
#include "cocaine/helpers/uri.hpp"

namespace cocaine { namespace plugin {

class mysql_t:
    public source_t
{
    public:
        static source_t* create(const std::string& args) {
            return new mysql_t(args);
        }

    public:
        mysql_t(const std::string& args):
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

        virtual void invoke(
            callback_fn_t callback,
            const std::string& method,
            const void* request = NULL,
            size_t request_size = 0) 
        {
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

            Json::FastWriter writer;
            std::string response(writer.write(result));
            
            callback(response.data(), response.size());
        }

    private:
        std::string m_host, m_username, m_password, m_db;
        unsigned int m_port, m_connect_timeout, m_read_timeout, m_write_timeout;
};

static const source_info_t plugin_info[] = {
    { "mysql", &mysql_t::create },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        return plugin_info;
    }
}

}}
