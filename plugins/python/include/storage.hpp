//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_PYTHON_PLUGIN_STORAGE_HPP
#define COCAINE_PYTHON_PLUGIN_STORAGE_HPP

#include "common.hpp"

namespace cocaine { namespace plugin {

class storage_object_t {
    public:
        PyObject_HEAD

        static PyObject* allocate(PyTypeObject* type, PyObject* args, PyObject* kwargs);
        static void deallocate(storage_object_t* self);
        static int initialize(storage_object_t* self, PyObject* args, PyObject* kwargs);

    public:
        static PyObject* get(storage_object_t* self, PyObject* args, PyObject* kwargs);
        static PyObject* set(storage_object_t* self, PyObject* args, PyObject* kwargs);
        static PyObject* get_id(storage_object_t* self, void* closure);

    private:
        static const char *get_kwlist[];
        static const char *set_kwlist[];

    private:
        PyObject* storage_id;
};

static PyMethodDef storage_object_methods[] = {
    { "get", (PyCFunction)storage_object_t::get,
        METH_VARARGS | METH_KEYWORDS,
        "Fetches the value for the specified key" },
    { "set", (PyCFunction)storage_object_t::set,
        METH_VARARGS | METH_KEYWORDS,
        "Stores the value for the specified key" },
    { NULL }
};

static PyGetSetDef storage_object_accessors[] = {
    { "id", (getter)storage_object_t::get_id, NULL, "storage id", NULL },
    { NULL }
};

static PyTypeObject storage_object_type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "Store",                                    /* tp_name */
    sizeof(storage_object_t),                   /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)storage_object_t::deallocate,   /* tp_dealloc */
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
    "Persistent data storage",                  /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    storage_object_methods,                     /* tp_methods */
    0,                                          /* tp_members */
    storage_object_accessors,                   /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)storage_object_t::initialize,     /* tp_init */
    0,                                          /* tp_alloc */
    storage_object_t::allocate                  /* tp_new */
};    

}}

#endif
