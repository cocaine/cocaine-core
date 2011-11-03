#ifndef COCAINE_PYTHON_WSGI_HPP
#define COCAINE_PYTHON_WSGI_HPP

#include "http_parser.h"

#include "python+raw.hpp"

namespace cocaine { namespace plugin {

class wsgi_python_t:
    public raw_python_t
{
    public:
        static source_t* create(const std::string& args);

    public:
        wsgi_python_t(const std::string& args):
            raw_python_t(args)
        { }

    private:
        virtual void invoke(callback_fn_t callback,
                            const std::string& method, 
                            const void* request,
                            size_t size);
        
        object_t parse(const void* request, size_t size);
};

class parser_t {
    public:
        parser_t(const void* request, size_t size);
        object_t environment();

    private:
        void set(const std::string& key, const std::string& value);
        
        static int on_message_begin(http_parser*);
        static int on_url(http_parser*, const char*, size_t);
        static int on_header_field(http_parser*, const char*, size_t);
        static int on_header_value(http_parser*, const char*, size_t);
        static int on_headers_complete(http_parser*);
        static int on_body(http_parser*, const char*, size_t);
        static int on_message_complete(http_parser*);

    private:
        http_parser_settings m_settings;
        http_parser m_parser;

        object_t m_environment;
};

}}

#endif
