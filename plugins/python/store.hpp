#include "storage.hpp"
#include "digest.hpp"

namespace yappi { namespace plugin {
    struct store_object_t {
        PyObject_HEAD
        
        static void deallocate(store_object_t* self) {
            self->ob_type->tp_free(reinterpret_cast<PyObject*>(self));
        }
        
        static PyObject* allocate(PyTypeObject* type, PyObject* args, PyObject* kwargs) {
            store_object_t* self = reinterpret_cast<store_object_t*>(type->tp_alloc(type, 0));
            
            if(self) {
                // Do something useful
            }
            
            return reinterpret_cast<PyObject*>(self);
        }
        
        static int initialize(store_object_t* self, PyObject* args, PyObject* kwargs) {
            return 0;
        }
    } store_object_t;
    
    // static PyMemberDef store_object_members[] = {
    //     { NULL }
    // };
    
    static PyMethodDef store_object_methods[] = {
        { NULL }
    };
    
    static PyTypeObject store_object_type = {
        PyObject_HEAD_INIT(NULL)
        0,                                      /* ob_size */
        "context.Store",                        /* tp_name */
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
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
        "Per-thread persistent data store",     /* tp_doc */
        0,                                      /* tp_traverse */
        0,                                      /* tp_clear */
        0,                                      /* tp_richcompare */
        0,                                      /* tp_weaklistoffset */
        0,                                      /* tp_iter */
        0,                                      /* tp_iternext */
        store_object_methods,                   /* tp_methods */
        0, // store_object_members,                   /* tp_members */
        0,                                      /* tp_getset */
        0,                                      /* tp_base */
        0,                                      /* tp_dict */
        0,                                      /* tp_descr_get */
        0,                                      /* tp_descr_set */
        0,                                      /* tp_dictoffset */
        (initproc)store_object_t::initialize,   /* tp_init */
        0,                                      /* tp_alloc */
        store_object_t::allocate,               /* tp_new */
    };    
}}
