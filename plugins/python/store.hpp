#include "Python.h"

#include "cocaine/storage.hpp"

namespace cocaine { namespace plugin {

class store_object_t {
    public:
        PyObject_HEAD

        static PyObject* allocate(PyTypeObject* type, PyObject* args, PyObject* kwargs);
        static void deallocate(store_object_t* self);
        static int initialize(store_object_t* self, PyObject* args, PyObject* kwargs);

    public:
        static PyObject* get(store_object_t* self, PyObject* args, PyObject* kwargs);
        static PyObject* set(store_object_t* self, PyObject* args, PyObject* kwargs);
        static PyObject* get_id(store_object_t* self, void* closure);

    private:
        static const char *get_kwlist[];
        static const char *set_kwlist[];

    private:
        PyObject* store_id;
};

static PyMethodDef store_object_methods[] = {
    { "get", (PyCFunction)store_object_t::get, METH_VARARGS | METH_KEYWORDS,
        "Fetches the value for the specified key" },
    { "set", (PyCFunction)store_object_t::set, METH_VARARGS | METH_KEYWORDS,
        "Stores the value for the specified key" },
    { NULL }
};

static PyGetSetDef store_object_accessors[] = {
    { "id", (getter)store_object_t::get_id, NULL, "store id", NULL },
    { NULL }
};

static PyTypeObject store_object_type = {
    PyObject_HEAD_INIT(NULL)
    0,                                      /* ob_size */
    "Store",                                /* tp_name */
    sizeof(store_object_t),                 /* tp_basicsize */
    0,                                      /* tp_itemsize */
    (destructor)store_object_t::deallocate, /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    0,                                      /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    0,                                      /* tp_hash */
    0,                                      /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                     /* tp_flags */
    "Per-thread persistent data store",     /* tp_doc */
    0,                                      /* tp_traverse */
    0,                                      /* tp_clear */
    0,                                      /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    0,                                      /* tp_iter */
    0,                                      /* tp_iternext */
    store_object_methods,                   /* tp_methods */
    0,                                      /* tp_members */
    store_object_accessors,                 /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    (initproc)store_object_t::initialize,   /* tp_init */
    0,                                      /* tp_alloc */
    store_object_t::allocate                /* tp_new */
};    

}}
