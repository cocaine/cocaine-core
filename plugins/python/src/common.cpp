#include "common.hpp"

using namespace cocaine::plugin;

std::string python_support_t::exception() {
    object_t type(NULL), object(NULL), traceback(NULL);
    
    PyErr_Fetch(&type, &object, &traceback);
    object_t message(PyObject_Str(object));
    
    return PyString_AsString(message);
}

Json::Value python_support_t::unwrap(PyObject* object) {
    Json::Value result;
    
    if(PyBool_Check(object)) {
        result = (object == Py_True ? true : false);
    } else if(PyInt_Check(object) || PyLong_Check(object)) {
        result = static_cast<Json::Int>(PyInt_AsLong(object));
    } else if(PyFloat_Check(object)) {
        result = PyFloat_AsDouble(object);
    } else if(PyString_Check(object)) {
        result = PyString_AsString(object);
    } else if(PyDict_Check(object)) {
        // Borrowed references, so no need to track them
        PyObject *key, *value;
        Py_ssize_t position = 0;
        
        // Iterate and convert everything to strings
        while(PyDict_Next(object, &position, &key, &value)) {
            result[PyString_AsString(PyObject_Str(key))] = unwrap(value); 
        }
    } else if(object_t iterator = PyObject_GetIter(object)) {
        object_t item(NULL);

        while(item = PyIter_Next(iterator)) {
            result.append(unwrap(item));
        }
    } else if(PyErr_Clear(), object != Py_None) {
        result["error"] = "<error: unrecognized type>";
    }    

    return result;
}

PyObject* python_support_t::wrap(const Json::Value& value) {
    PyObject* object = NULL;

    switch(value.type()) {
        case Json::booleanValue:
            return PyBool_FromLong(value.asBool());
        case Json::intValue:
        case Json::uintValue:
            return PyLong_FromLong(value.asInt());
        case Json::realValue:
            return PyFloat_FromDouble(value.asDouble());
        case Json::stringValue:
            return PyString_FromString(value.asCString());
        case Json::objectValue: {
            object = PyDict_New();
            Json::Value::Members names(value.getMemberNames());

            for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
                PyDict_SetItemString(object, it->c_str(), wrap(value[*it]));
            }

            return object;
        } case Json::arrayValue: {
            object = PyTuple_New(value.size());
            Py_ssize_t position = 0;

            for(Json::Value::const_iterator it = value.begin(); it != value.end(); ++it) {
                PyTuple_SET_ITEM(object, position++, wrap(*it));
            }

            return object;
        } case Json::nullValue:
            Py_RETURN_NONE;
    }
}
