/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

namespace cocaine {
namespace hpack {

struct headers {
    friend struct header_static_table_t;

private:
    struct detail {
        struct empty_placeholder {
            static
            const std::string&
            name() {
                static std::string data;
                return data;
            }

            static
            const std::string&
            value() {
                static std::string data;
                return data;
            }
        };
    };

public:
    struct default_values_t {
        struct zero_uint_value_t {
            static
            const std::string&
            value() {
                static std::string data(8, '\0');
                return data;
            }
        };

        struct false_bool_value_t {
            static
            const std::string&
            value() {
                static std::string data("0");
                return data;
            }
        };

        struct empty_string_value_t {
            static
            const std::string&
            value() {
                static std::string data;
                return data;
            }
        };

        struct get_value_t {
            static
            const std::string&
            value() {
                static std::string data("GET");
                return data;
            }
        };

        struct post_value_t {
            static
            const std::string&
            value() {
                static std::string data("POST");
                return data;
            }
        };

        struct root_value_t {
            static
            const std::string&
            value() {
                static std::string data("/");
                return data;
            }
        };

        struct index_value_t {
            static
            const std::string&
            value() {
                static std::string data("/index.html");
                return data;
            }
        };

        struct http_value_t {
            static
            const std::string&
            value() {
                static std::string data("http");
                return data;
            }
        };

        struct https_value_t {
            static
            const std::string&
            value() {
                static std::string data("https");
                return data;
            }
        };

        struct status_200_value_t {
            static
            const std::string&
            value() {
                static std::string data("200");
                return data;
            }
        };

        struct status_204_value_t {
            static
            const std::string&
            value() {
                static std::string data("204");
                return data;
            }
        };

        struct status_206_value_t {
            static
            const std::string&
            value() {
                static std::string data("206");
                return data;
            }
        };

        struct status_304_value_t {
            static
            const std::string&
            value() {
                static std::string data("304");
                return data;
            }
        };

        struct status_400_value_t {
            static
            const std::string&
            value() {
                static std::string data("400");
                return data;
            }
        };

        struct status_404_value_t {
            static
            const std::string&
            value() {
                static std::string data("404");
                return data;
            }
        };

        struct status_500_value_t {
            static
            const std::string&
            value() {
                static std::string data("500");
                return data;
            }
        };

        struct gzip_value_t {
            static
            const std::string&
            value() {
                static std::string data("gzip, deflate");
                return data;
            }
        };
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct authority:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data(":authority");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct method:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data(":method");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct path:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data(":path");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct scheme:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data(":scheme");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct status:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data(":status");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept_charset:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("accept-charset");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept_encoding:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("accept-encoding");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept_language:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("accept-language");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept_ranges:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("accept-ranges");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("accept");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct access_control_allow_origin:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("access-control-allow-origin");
                return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct age:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("age");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct allow:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("allow");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct authorization:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("authorization");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct cache_control:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("cache-control");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_disposition:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("content-disposition");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_encoding:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("content-encoding");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_language:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("content-language");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_length:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("content-length");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_location:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("content-location");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_range:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("content-range");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_type:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("content-type");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct cookie:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("cookie");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct date:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("date");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct etag:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("etag");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct expect:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("expect");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct expires:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("expires");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct from:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("from");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct host:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("host");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_match:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("if-match");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_modified_since:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("if-modified-since");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_none_match:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("if-none-match");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_range:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("if-range");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_unmodified_since:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("if-unmodified-since");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct last_modified:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("last-modified");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct link:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("link");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct location:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("location");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct max_forwards:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("max-forwards");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct proxy_authenticate:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("proxy-authenticate");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct proxy_authorization:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("proxy-authorization");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct range:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("range");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct referer:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("referer");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct refresh:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("refresh");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct retry_after:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("retry-after");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct server:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("server");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct set_cookie:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("set-cookie");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct strict_transport_security:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("strict-transport-security");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct transfer_encoding:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("transfer-encoding");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct user_agent:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("user-agent");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct vary:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("vary");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct via:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("via");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct www_authenticate:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("www-authenticate");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct span_id:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("span_id");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct trace_id:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("trace_id");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct parent_id:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("parent_id");
            return data;
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct trace_bit:
        public DefaultValue
    {
        static
        const std::string&
        name() {
            static std::string data("trace_bit");
            return data;
        }
    };
};

} //  namespace hpack
} //  namespace cocaine
