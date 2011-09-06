#if PY_VERSION_HEX > 0x02070000
    #include <pycapsule.h>
#endif

#include "storage.hpp"
#include "digest.hpp"

namespace yappi { namespace plugin {
    class store_object_t {
        public:
            PyObject_HEAD

            static void deallocate(store_object_t* self) {
                delete self->store_id;
                self->ob_type->tp_free(reinterpret_cast<PyObject*>(self));
            }
            
            static int initialize(store_object_t* self, PyObject* args, PyObject* kwargs) {
                object_t source(NULL);

                if(!PyArg_ParseTuple(args, "O", &source)) {
                    return -1;
                }

#if PY_VERSION_HEX > 0x02070000
                if(!PyCapsule_CheckExact(source)) {
#else
                if(!PyCObject_Check(source)) {
#endif
                    PyErr_SetString(PyExc_RuntimeError,
                        "This class cannot be instantiated directly");
                    return -1;
                }

#if PY_VERSION_HEX > 0x02070000
                std::string uri = static_cast<source_t*>(
                    PyCapsule_GetPointer(source, NULL))->uri();
#else
                std::string uri = static_cast<source_t*>(
                    PyCObject_AsVoidPtr(source))->uri();
#endif

                self->store_id = new std::string(security::digest_t().get(uri));

                return 0;
            }

        public:
            static PyObject* get(store_object_t* self, PyObject* args, PyObject* kwargs) {
                Json::Value store;
                PyObject* key;

                if(!PyArg_ParseTupleAndKeywords(args, kwargs, "S", const_cast<char**>(get_kwlist), &key)) {
                    return NULL;
                }

                Py_BEGIN_ALLOW_THREADS
                    store = storage::storage_t::instance()->get(*self->store_id);
                Py_END_ALLOW_THREADS
                
                Json::Value value = store["store"][PyString_AsString(key)];

                if(!value.empty()) {
                    if(value.isBool()) {
                        return PyBool_FromLong(value.asBool());
                    } else if(value.isIntegral()) {
                        return PyLong_FromLong(value.asInt());
                    } else if(value.isDouble()) {
                        return PyFloat_FromDouble(value.asDouble());
                    } else if(value.isString()) {
                        return PyString_FromString(value.asCString());
                    } else {
                        PyErr_SetString(PyExc_TypeError,
                            "Invalid storage data format");
                        return NULL;
                    }
                } else {
                    Py_RETURN_NONE;
                }
            }

            static PyObject* set(store_object_t* self, PyObject* args, PyObject* kwargs) {
                bool result;
                Json::Value store, object;
                PyObject *key, *value;
                
                if(!PyArg_ParseTupleAndKeywords(args, kwargs, "SO", const_cast<char**>(set_kwlist), &key, &value)) {
                    return NULL;
                }
               
                if(PyBool_Check(value)) {
                    object = (value == Py_True ? true : false);
                } else if(PyInt_Check(value)) {
                    object = static_cast<Json::Int>(PyInt_AsLong(value));
                } else if(PyLong_Check(value)) {
                    object = static_cast<Json::Int>(PyLong_AsLong(value));
                } else if(PyFloat_Check(value)) {
                    object = PyFloat_AsDouble(value);
                } else if(PyString_Check(value)) {
                    object = PyString_AsString(value);
                } else {
                    PyErr_SetString(PyExc_TypeError,
                        "Only primitive types are allowed");
                    return NULL;
                }

                Py_BEGIN_ALLOW_THREADS
                    store = storage::storage_t::instance()->get(*self->store_id);
                Py_END_ALLOW_THREADS
              
                store["store"][PyString_AsString(key)] = object;

                Py_BEGIN_ALLOW_THREADS
                    result = storage::storage_t::instance()->put(*self->store_id, store);
                Py_END_ALLOW_THREADS

                if(result) {
                    Py_RETURN_TRUE;
                } else {
                    Py_RETURN_FALSE;
                }
            }

        private:
            static const char *get_kwlist[];
            static const char *set_kwlist[];

        private:
            std::string* store_id;       
    };

    const char* store_object_t::get_kwlist[] = { "key", NULL };
    const char* store_object_t::set_kwlist[] = { "key", "value", NULL };
    
    static PyMethodDef store_object_methods[] = {
        { "get", (PyCFunction)store_object_t::get, METH_VARARGS | METH_KEYWORDS,
            "Fetches the value for the specified key" },
        { "set", (PyCFunction)store_object_t::set, METH_VARARGS | METH_KEYWORDS,
            "Stores the value for the specified key" },
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
        0,                                      /* tp_getset */
        0,                                      /* tp_base */
        0,                                      /* tp_dict */
        0,                                      /* tp_descr_get */
        0,                                      /* tp_descr_set */
        0,                                      /* tp_dictoffset */
        (initproc)store_object_t::initialize,   /* tp_init */
        0,                                      /* tp_alloc */
        PyType_GenericNew                       /* tp_new */
    };    
}}
