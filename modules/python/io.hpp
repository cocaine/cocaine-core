//
// Copyright (C) 2011-2012 Alexander Eliseev <admin@inkvi.com>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef COCAINE_PYTHON_PLUGIN_IO_HPP
#define COCAINE_PYTHON_PLUGIN_IO_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include "Python.h"

namespace cocaine { namespace engine {
	
class io_t;
	
class python_io_t {
    public:
        PyObject_HEAD

        static int constructor(python_io_t* self, PyObject* args, PyObject* kwargs);
        static void destructor(python_io_t* self);

        static PyObject* read(python_io_t* self, PyObject* args);
        static PyObject* write(python_io_t* self, PyObject* args);

    public:
        io_t* io;
};

static PyMethodDef python_io_object_methods[] = {
    { "read", (PyCFunction) python_io_t::read,
        METH_VARARGS, "" },
    { "write", (PyCFunction) python_io_t::write,
        METH_VARARGS, "" },
    { NULL }
};

static PyTypeObject python_io_object_type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "cocaine.modules.python.io",                /* tp_name */
    sizeof(python_io_t),                       /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)python_io_t::destructor,       /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    "Python IO wrapper",                        /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    python_io_object_methods,                         /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)python_io_t::constructor,        /* tp_init */
    0,                                          /* tp_alloc */
    PyType_GenericNew                           /* tp_new */
};    

}}
#endif
