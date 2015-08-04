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

struct headers {
    friend struct header_static_table_t;

private:
    struct detail {
        struct empty_placeholder {
            static
            constexpr
            header::data_t
            name() {
                return header::create_data("");
            }

            static
            constexpr
            header::data_t
            value() {
                return header::create_data("");
            }
        };
        template <class Value>
        struct value_mixin {
            static
            constexpr
            header::data_t
            value() {
                return Value::value();
            }
        };
    };

public:
    template<class Header>
    static
    header_t
    make_header() {
        return header_t(Header::name(), Header::value());
    }

    struct default_values_t {
        struct zero_uint_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("\0\0\0\0\0\0\0\0");
            }
        };

        struct empty_string_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("");
            }
        };

        struct get_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("GET");
            }
        };

        struct post_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("POST");
            }
        };

        struct root_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("/");
            }
        };

        struct index_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("/index.html");
            }
        };

        struct http_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("http");
            }
        };

        struct https_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("https");
            }
        };

        struct status_200_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("200");
            }
        };

        struct status_204_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("204");
            }
        };

        struct status_206_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("206");
            }
        };

        struct status_304_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("304");
            }
        };

        struct status_400_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("400");
            }
        };

        struct status_404_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("404");
            }
        };

        struct status_500_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("500");
            }
        };

        struct gzip_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("gzip, deflate");
            }
        };
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct authority:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data(":authority");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct method:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data(":method");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct path:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data(":path");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct scheme:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data(":scheme");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct status:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data(":status");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept_charset:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("accept-charset");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept_encoding:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("accept-encoding");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept_language:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("accept-language");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept_ranges:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("accept-ranges");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct accept:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("accept");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct access_control_allow_origin:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("access-control-allow-origin");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct age:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("age");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct allow:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("allow");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct authorization:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("authorization");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct cache_control:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("cache-control");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_disposition:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("content-disposition");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_encoding:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("content-encoding");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_language:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("content-language");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_length:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("content-length");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_location:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("content-location");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_range:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("content-range");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct content_type:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("content-type");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct cookie:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("cookie");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct date:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("date");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct etag:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("etag");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct expect:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("expect");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct expires:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("expires");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct from:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("from");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct host:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("host");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_match:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("if-match");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_modified_since:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("if-modified-since");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_none_match:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("if-none-match");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_range:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("if-range");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct if_unmodified_since:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("if-unmodified-since");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct last_modified:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("last-modified");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct link:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("link");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct location:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("location");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct max_forwards:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("max-forwards");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct proxy_authenticate:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("proxy-authenticate");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct proxy_authorization:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("proxy-authorization");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct range:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("range");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct referer:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("referer");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct refresh:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("refresh");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct retry_after:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("retry-after");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct server:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("server");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct set_cookie:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("set-cookie");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct strict_transport_security:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("strict-transport-security");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct transfer_encoding:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("transfer-encoding");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct user_agent:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("user-agent");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct vary:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("vary");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct via:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("via");
        }
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct www_authenticate:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("www-authenticate");
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct span_id:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("span_id");
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct trace_id:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("trace_id");
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct parent_id:
        public detail::value_mixin<DefaultValue>
    {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("parent_id");
        }
    };
};
