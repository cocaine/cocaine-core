#include "python.hpp"
#include "io.hpp"


using namespace cocaine::engine;


int python_io_t::constructor(python_io_t* self, PyObject* args, PyObject* kwargs) {
    
    PyObject * py_io;

    if(!PyArg_ParseTuple(args, "O", &py_io))
        return NULL;

    self->io = static_cast<io_t*>(PyCObject_AsVoidPtr(py_io));

    Py_DECREF(py_io);
          
    return 0;
}

void python_io_t::destructor(python_io_t* self) {
    self->ob_type->tp_free(self);    
}

PyObject* python_io_t::read(python_io_t* self, PyObject* args) {
    if (!self->io->request || !self->io->request_size)
        Py_RETURN_NONE;

    python_object_t string(PyString_FromStringAndSize(self->io->pull(), self->io->request_size));
    return string;
}

PyObject* python_io_t::write(python_io_t* self, PyObject* args) {    
    const char * message;
    int size;

    if(!PyArg_ParseTuple(args, "s#", &message, &size))
        return NULL;

    self->io->push(message, size);

    Py_RETURN_NONE;
}