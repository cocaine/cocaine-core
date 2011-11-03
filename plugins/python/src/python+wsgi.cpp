#include "boost/format.hpp"

#include "python+wsgi.hpp"

using namespace cocaine::plugin;

source_t* wsgi_python_t::create(const std::string& args) {
    return new wsgi_python_t(args);
}

void wsgi_python_t::invoke(
    callback_fn_t callback,
    const std::string& method,
    const void* request,
    size_t size) 
{
    thread_state_t state;
    object_t object(PyObject_GetAttrString(m_module, method.c_str()));
    
    if(PyErr_Occurred()) {
        exception();
    } else if(!PyCallable_Check(object)) {
        throw std::runtime_error("'" + method + "' is not callable");
    } 
    
    if(PyType_Check(object)) {
        if(PyType_Ready(reinterpret_cast<PyTypeObject*>(*object)) != 0) {
            exception();
        }
    }
    
    parser_t parser(request, size);
    object_t args(PyTuple_Pack(1, *parser.environment()));
    object_t result(PyObject_Call(object, args, NULL));

    if(PyErr_Occurred()) {
        exception();
    } else if(result.valid()) {
        respond(callback, result);
    }
}

parser_t::parser_t(const void* request, size_t size):
    m_environment(PyDict_New())
{
    m_settings.on_message_begin = &on_message_begin;
    m_settings.on_url = &on_url;
    m_settings.on_header_field = &on_header_field;
    m_settings.on_header_value = &on_header_value;
    m_settings.on_headers_complete = &on_headers_complete;
    m_settings.on_body = &on_body;
    m_settings.on_message_complete = &on_message_complete;

    http_parser_init(&m_parser, HTTP_REQUEST);
    m_parser.data = this;

    if(http_parser_execute(&m_parser, &m_settings, 
        static_cast<const char*>(request), size) != size)
    {
        throw std::runtime_error(http_errno_description(
            HTTP_PARSER_ERRNO(&m_parser)));
    }

    PyDict_SetItemString(m_environment, "wsgi.version",
        Py_BuildValue("(ii)", 1, 0));
    
    PyDict_SetItemString(m_environment, "wsgi.url_scheme",
        PyString_FromString("http"));

    Py_INCREF(Py_True);
    PyDict_SetItemString(m_environment, "wsgi.multithread", Py_True);

    Py_INCREF(Py_True);
    PyDict_SetItemString(m_environment, "wsgi.multiprocess", Py_True);

    Py_INCREF(Py_False);
    PyDict_SetItemString(m_environment, "wsgi.run_once", Py_False);
}

object_t parser_t::environment() {
    return m_environment;
}

int parser_t::on_message_begin(http_parser* parser) {
    return 0;
}

int parser_t::on_url(http_parser* parser, const char* at, size_t length) {
    parser_t* p = static_cast<parser_t*>(parser->data);

    PyDict_SetItemString(p->m_environment, "REQUEST_METHOD", 
        PyString_FromString(
            http_method_str(static_cast<http_method>(parser->method))
        ));

    PyDict_SetItemString(p->m_environment, "SERVER_PROTOCOL",
        PyString_FromString(
            (boost::format("HTTP/%1%.%2%") 
                % parser->http_major 
                % parser->http_minor
            ).str().c_str()
        ));

    // FIXME: Have to specify something special here to work in a cloud
    PyDict_SetItemString(p->m_environment, "SERVER_NAME",
        PyString_FromString("cocaine"));
    
    PyDict_SetItemString(p->m_environment, "SERVER_PORT", 
        PyString_FromString("80"));

    return 0;
}

int parser_t::on_header_field(http_parser* parser, const char* at, size_t length) {
    return 0;
}

int parser_t::on_header_value(http_parser* parser, const char* at, size_t length) {
    return 0;
}

int parser_t::on_headers_complete(http_parser* parser) {
    return 0;
}

int parser_t::on_body(http_parser* parser, const char* at, size_t length) {
    return 0;
}

int parser_t::on_message_complete(http_parser* parser) {
    return 0;
}
