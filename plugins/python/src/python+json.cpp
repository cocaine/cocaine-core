#include "python+json.hpp"

using namespace cocaine::plugin;

source_t* python_json_t::create(const std::string& args) {
    return new python_json_t(args);
}

void python_json_t::respond(
    callback_fn_t callback,
    object_t& result)
{
    Json::FastWriter writer;
    Json::Value object(convert(result));

    Py_BEGIN_ALLOW_THREADS
        std::string response(writer.write(object));
        callback(response.data(), response.size());
    Py_END_ALLOW_THREADS
}

Json::Value python_json_t::convert(PyObject* result) {
    Json::Value object;
    
    if(PyBool_Check(result)) {
        object = (result == Py_True ? true : false);
    } else if(PyInt_Check(result) || PyLong_Check(result)) {
        object = static_cast<Json::Int>(PyInt_AsLong(result));
    } else if(PyFloat_Check(result)) {
        object = PyFloat_AsDouble(result);
    } else if(PyString_Check(result)) {
        object = PyString_AsString(result);
    } else if(PyDict_Check(result)) {
        // Borrowed references, so no need to track them
        PyObject *key, *value;
        Py_ssize_t position = 0;
        
        // Iterate and convert everything to strings
        while(PyDict_Next(result, &position, &key, &value)) {
            object[PyString_AsString(PyObject_Str(key))] = convert(value); 
        }
    } else { 
        object_t iterator = PyObject_GetIter(result);

        if(iterator.valid()) {
            object_t item(NULL);

            while(item = PyIter_Next(iterator)) {
                if(PyErr_Occurred()) {
                    exception();
                }
                
                object.append(convert(item));
            }
        } else {
            PyErr_Clear();

            if(object != Py_None) {
                throw std::runtime_error("unable to serialize the result");
            }
        }
    }    

    return object;
}
