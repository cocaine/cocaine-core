#include <Python.h>

#include <stdexcept>
#include "plugin.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error
// * std::invalid_argument

class python_t: public source_t {
    public:
        python_t(const std::string& uri):
            m_target(uri.substr(uri.find_first_of(":") + 3)) {}

        virtual dict_t fetch() {
            dict_t dict;
            FILE *file = fopen(m_target.c_str(), "r");
            
            if(!file) {
                dict["error"] = strerror(errno);
                return dict;
            }
            
            // Getting the thread state
            PyGILState_STATE state = PyGILState_Ensure();
                
            PyObject *globals = PyDict_New();
            PyObject *locals = PyDict_New();
            PyObject *name = PyString_FromString("__main__");
            PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
            PyDict_SetItemString(globals, "__name__", name);

            // Run the script
            PyRun_FileEx(
                file,
                "__yappi__",
                Py_file_input,
                globals, locals, true);

            if(PyErr_Occurred()) {
                PyObject *type, *message, *traceback;
                PyErr_Fetch(&type, &message, &traceback);
                    
                PyObject *typestr = PyObject_Str(type);
                PyObject *messagestr = PyObject_Str(message);

                dict["exception:type"] = PyString_AsString(typestr);
                dict["exception:message"] = PyString_AsString(messagestr);

                Py_DecRef(typestr);
                Py_DecRef(messagestr);
                Py_DecRef(type);
                Py_DecRef(message);
                Py_DecRef(traceback);
            } else {
                PyObject *result = PyDict_GetItemString(locals, "result");

                if(result) {
                    if(PyDict_Check(result)) {
                        PyObject *key, *value;
                        PyObject *keystr, *valuestr;
                        Py_ssize_t position = 0;

                        while(PyDict_Next(result, &position, &key, &value)) {
                            keystr = PyObject_Str(key);
                            valuestr = PyObject_Str(value);

                            dict.insert(std::make_pair(PyString_AsString(keystr), PyString_AsString(valuestr)));

                            Py_DecRef(keystr);
                            Py_DecRef(valuestr);
                        }
                    }
                
                    Py_DecRef(result);
                }
            }
                
            // Cleanup
            Py_DecRef(globals);
            Py_DecRef(locals);
            Py_DecRef(name);

            PyGILState_Release(state);
            
            return dict;
        }

    private:
        std::string m_target;
};

void* create_python_instance(const char* uri) {
    return new python_t(uri);
}

static const plugin_info_t plugin_info = {
    1,
    {
        { "python", &create_python_instance }
    }
};

extern "C" {
    const plugin_info_t* get_plugin_info() {
        // This is called in the main thread
        // during registry initialization
        Py_InitializeEx(0);
        PyEval_InitThreads();
        PyEval_ReleaseLock();

        return &plugin_info;
    }
}
