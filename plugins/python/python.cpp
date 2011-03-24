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
            m_path(uri.substr(uri.find_first_of(":") + 3))
        {
            Py_InitializeEx(0);
            m_state = Py_NewInterpreter();
        }

        ~python_t() {
            PyEval_RestoreThread(m_state);
            Py_EndInterpreter(m_state);
        }

        virtual dict_t fetch() {
            dict_t dict;
            
            FILE *file = fopen(m_path.c_str(), "r");
            if(!file) {
                dict["error"] = strerror(errno);
            } else {
                PyEval_RestoreThread(m_state);
                
                PyObject *locals = PyDict_New();
                PyObject *globals = PyDict_New();
                PyObject *module = PyString_FromString("__main__");
                PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
                PyDict_SetItemString(globals, "__module__", module);

                // Run the script
                PyRun_FileEx(
                    file,
                    "__yappi__",
                    Py_file_input,
                    locals, globals, true);

                Py_DecRef(locals);

                if(PyErr_Occurred()) {
                    PyObject *type, *message, *traceback;
                    PyErr_Fetch(&type, &message, &traceback);
                    
                    // Don't need it anyway
                    Py_DecRef(traceback);

                    PyObject *typestr = PyObject_Str(type);
                    PyObject *messagestr = PyObject_Str(message);

                    dict["exception:type"] = PyString_AsString(typestr);
                    dict["exception:message"] = PyString_AsString(messagestr);

                    Py_DecRef(messagestr);
                    Py_DecRef(typestr);
                    Py_DecRef(message);
                    Py_DecRef(type);
                } else {
                    PyObject *result = PyMapping_GetItemString(globals, "result");

                    if(PyMapping_Check(result)) {
                        PyObject *items = PyMapping_Items(result);
                        PyObject *iterator = PyObject_GetIter(items);
                        PyObject *tuple;

                        while((tuple = PyIter_Next(iterator))) {
                            PyObject *key, *value, *keystr, *valuestr;
                            key = PyTuple_GetItem(tuple, 0);
                            value = PyTuple_GetItem(tuple, 1);

                            keystr = PyObject_Str(key);
                            valuestr = PyObject_Str(value);

                            dict.insert(std::make_pair(PyString_AsString(keystr), PyString_AsString(valuestr)));

                            Py_DecRef(valuestr);
                            Py_DecRef(keystr);
                            Py_DecRef(value);
                            Py_DecRef(key);
                            Py_DecRef(tuple);
                        }

                        Py_DecRef(iterator);
                        Py_DecRef(items);
                    } else if(result) {
                        PyObject* str = PyObject_Str(result);
                        dict["result"] = PyString_AsString(str);

                        Py_DecRef(str);
                    }
                
                    Py_DecRef(result);
                }
                
                // Cleanup
                Py_DecRef(module);
                Py_DecRef(globals);

                m_state = PyEval_SaveThread();
            }
            
            return dict;
        }

    private:
        std::string m_path;

        PyThreadState* m_state;
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
        return &plugin_info;
    }
}
