
#pragma once

#include "cocaine/hpack/mpl.hpp"

#include "cocaine/hpack/header_definitions.hpp"

namespace cocaine { namespace hpack {

struct header_static_table_t {
    typedef boost::mpl::vector83 <
        headers::detail::empty_placeholder, // 0. Reserved
        headers::authority<>,
        headers::method<headers::default_values_t::get_value_t>,
        headers::method<headers::default_values_t::post_value_t>,
        headers::path<headers::default_values_t::root_value_t>,
        headers::path<headers::default_values_t::index_value_t>,
        headers::scheme<headers::default_values_t::http_value_t>,
        headers::scheme<headers::default_values_t::https_value_t>,
        headers::status<headers::default_values_t::status_200_value_t>,
        headers::status<headers::default_values_t::status_204_value_t>,
        headers::status<headers::default_values_t::status_206_value_t>,
        headers::status<headers::default_values_t::status_304_value_t>,
        headers::status<headers::default_values_t::status_400_value_t>,
        headers::status<headers::default_values_t::status_404_value_t>,
        headers::status<headers::default_values_t::status_500_value_t>,
        headers::accept_charset<>,
        headers::accept_encoding<headers::default_values_t::gzip_value_t>,
        headers::accept_language<>,
        headers::accept_ranges<>,
        headers::accept<>,
        headers::access_control_allow_origin<>,
        headers::age<>,
        headers::allow<>,
        headers::authorization<>,
        headers::cache_control<>,
        headers::content_disposition<>,
        headers::content_encoding<>,
        headers::content_language<>,
        headers::content_length<>,
        headers::content_location<>,
        headers::content_range<>,
        headers::content_type<>,
        headers::cookie<>,
        headers::date<>,
        headers::etag<>,
        headers::expect<>,
        headers::expires<>,
        headers::from<>,
        headers::host<>,
        headers::if_match<>,
        headers::if_modified_since<>,
        headers::if_none_match<>,
        headers::if_range<>,
        headers::if_unmodified_since<>,
        headers::last_modified<>,
        headers::link<>,
        headers::location<>,
        headers::max_forwards<>,
        headers::proxy_authenticate<>,
        headers::proxy_authorization<>,
        headers::range<>,
        headers::referer<>,
        headers::refresh<>,
        headers::retry_after<>,
        headers::server<>,
        headers::set_cookie<>,
        headers::strict_transport_security<>,
        headers::transfer_encoding<>,
        headers::user_agent<>,
        headers::vary<>,
        headers::via<>,
        headers::www_authenticate<>, // 61
        // Reserved for http2 extensions
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder,
        headers::detail::empty_placeholder, // 79
        // Cocaine specific headers
        headers::trace_id<>,
        headers::span_id<>,
        headers::parent_id<>
    > headers_storage;

    static constexpr size_t size = boost::mpl::size<headers_storage>::type::value;
    typedef std::array <header_t, size> storage_t;

    static
    constexpr
    size_t
    get_size() {
        return size;
    }

    static
    const storage_t&
    get_headers();

    template<class Header>
    constexpr
    static
    size_t
    idx() {
        static_assert(boost::mpl::contains<headers_storage, Header>::type::value,
                      "Could not find header in statis table");
        return boost::mpl::find<headers_storage, Header>::type::pos::value;
    }
};

} //  namespace hpack
} //  namespace cocaine
