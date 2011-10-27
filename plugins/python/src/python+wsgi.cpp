#include "python+wsgi.hpp"

using namespace cocaine::plugin;

source_t* python_wsgi_t::create(const std::string& args) {
    return new python_wsgi_t(args);
}

void python_wsgi_t::respond(
    callback_fn_t callback,
    object_t& result)
{
    // NOTE: This is against the WSGI specifications, but it's better to warn
    // the user instead of sending the string one byte at a time.
    if(PyString_Check(result)) {
        throw std::runtime_error("the result must be an iterable");
    }

    object_t iterator(PyObject_GetIter(result));

    if(iterator.valid()) {
        object_t item(NULL);

        while(true) {
            item = PyIter_Next(iterator);

            if(PyErr_Occurred()) {
                exception();
            } else if(!item.valid()) {
                break;
            }
        
#if PY_VERSION_HEX > 0x02060000
            if(PyObject_CheckBuffer(item)) {
                boost::shared_ptr<Py_buffer> buffer(
                    static_cast<Py_buffer*>(malloc(sizeof(Py_buffer))),
                    free);

                if(PyObject_GetBuffer(item, buffer.get(), PyBUF_SIMPLE) == 0) {
                    Py_BEGIN_ALLOW_THREADS
                        callback(buffer->buf, buffer->len);
                    Py_END_ALLOW_THREADS
                    
                    PyBuffer_Release(buffer.get());
                } else {
                    throw std::runtime_error("unable to serialize the result");
                }
            }
#else
            if(PyString_Check(item)) {
                callback(PyString_AsString(item), PyString_Size(item));
            } else {
                throw std::runtime_error("unable to serialize the result");
            }
#endif
        }
    } else {
        exception();
    }
}
