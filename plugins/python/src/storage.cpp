#if PY_VERSION_HEX > 0x02070000
    #include <pycapsule.h>
#endif

#include "cocaine/plugin.hpp"
#include "cocaine/security/digest.hpp"
#include "cocaine/storages/abstract.hpp"

#include "storage.hpp"

using namespace cocaine::plugin;
using namespace cocaine::security;
using namespace cocaine::storage;

PyObject* storage_object_t::allocate(PyTypeObject* type, PyObject* args, PyObject* kwargs) {
    storage_object_t* self = reinterpret_cast<storage_object_t*>(type->tp_alloc(type, 0));

    if(self != NULL) {
        self->storage_id = PyString_FromString("");

        if(self->storage_id == NULL) {
            Py_DECREF(self);
            return NULL;
        }
    }

    return reinterpret_cast<PyObject*>(self);
}

void storage_object_t::deallocate(storage_object_t* self) {
    Py_XDECREF(self->storage_id);
    self->ob_type->tp_free(reinterpret_cast<PyObject*>(self));
}
            
int storage_object_t::initialize(storage_object_t* self, PyObject* args, PyObject* kwargs) {
    PyObject* source = NULL;

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
    std::string name(static_cast<source_t*>(
        PyCapsule_GetPointer(source, NULL))->name());
#else
    std::string name(static_cast<source_t*>(
        PyCObject_AsVoidPtr(source))->name());
#endif

    Py_DECREF(self->storage_id);
    self->storage_id = PyString_FromString(digest_t().get(name).c_str());

    return 0;
}

PyObject* storage_object_t::get(storage_object_t* self, PyObject* args, PyObject* kwargs) {
    std::string storage_id(PyString_AsString(self->storage_id)), error;
    Json::Value value;
    PyObject* key;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "S", const_cast<char**>(get_kwlist), &key)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
        try {
            value = storage_t::create()->get(storage_id, PyString_AsString(key));
        } catch(const std::runtime_error& e) {
            error = e.what();
        }
    Py_END_ALLOW_THREADS
    
    if(!error.empty()) {
        PyErr_SetString(PyExc_RuntimeError, error.c_str());
        return NULL;
    }

    return python_support_t::wrap(value);
}

PyObject* storage_object_t::set(storage_object_t* self, PyObject* args, PyObject* kwargs) {
    std::string storage_id(PyString_AsString(self->storage_id)), error;
    PyObject *key, *value;
    
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "SO", const_cast<char**>(set_kwlist), &key, &value)) {
        return NULL;
    }
   
    Json::Value object(python_support_t::unwrap(value));
    
    Py_BEGIN_ALLOW_THREADS
        try {
            storage_t::create()->put(storage_id, PyString_AsString(key), object);
        } catch(const std::runtime_error& e) {
            error = e.what();
        }
    Py_END_ALLOW_THREADS

    if(!error.empty()) {
        PyErr_SetString(PyExc_RuntimeError, error.c_str());
        return NULL;
    }

    Py_RETURN_TRUE;
}

PyObject* storage_object_t::get_id(storage_object_t* self, void* closure) {
    Py_INCREF(self->storage_id);
    return self->storage_id;
}

const char* storage_object_t::get_kwlist[] = { "key", NULL };
const char* storage_object_t::set_kwlist[] = { "key", "value", NULL };
