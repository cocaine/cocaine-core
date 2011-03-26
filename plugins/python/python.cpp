#include <Python.h>

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "plugin.hpp"
#include "track.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error
// * std::invalid_argument

using namespace yappi::plugins;
using namespace yandex::helpers;

class python_t: public source_t {
    public:
        typedef track<PyGILState_STATE, PyGILState_Release> thread_state_t;
        typedef track<PyObject*, Py_DecRef> object_t;
        
        python_t(const std::string& uri):
            m_code(NULL)
        {
            // uri: python:///home/kobolog/test.py/main?arg=val&arg2=val2
            m_path = "/home/kobolog/test.py";
            m_name = "test";
            m_function = "func";
            m_args["text"] = "abc";
            m_args["num"] = "10";

            // Try to open the file
            std::ifstream input;
            input.exceptions(std::ifstream::badbit | std::ifstream::failbit);
            
            try {
                input.open(m_path.c_str(), std::ifstream::in);
            } catch(const std::ifstream::failure& e) {
                throw std::invalid_argument("cannot open " + m_path);
            }

            // Read the code
            std::stringstream code;
            code << input.rdbuf();

            // Get the thread state
            thread_state_t state = PyGILState_Ensure();
            
            // Compile the source
            m_code = Py_CompileString(
                code.str().c_str(),
                m_path.c_str(),
                Py_file_input);

            if(PyErr_Occurred()) {
                throw std::runtime_error(exception());
            }

            // Validate the code object
            // It should be loadable as a module
            object_t module = PyImport_ExecCodeModule(
                const_cast<char*>(m_name.c_str()), m_code);
            
            if(PyErr_Occurred()) {
                throw std::runtime_error(exception());
            }

            // And the function specified have to be there
            if(!PyObject_HasAttrString(module, m_function.c_str())) {
                throw std::invalid_argument("function not found");
            }
        }

        virtual dict_t fetch() {
            // Get the thread state
            thread_state_t state = PyGILState_Ensure();

            // Importing the code as module
            // Doing this every time to avoid clashes with other plugin instances
            // No error checks needed here, as they are done in the constructor
            object_t module = PyImport_ExecCodeModule(
                const_cast<char*>(m_name.c_str()), m_code);
            object_t function = PyObject_GetAttrString(module, m_function.c_str());

            // Empty args and kwargs for the function
            object_t args = PyTuple_New(0);
            object_t kwargs = PyDict_New();

            for(dict_t::iterator it = m_args.begin(); it != m_args.end(); ++it) {
                object_t temp = PyString_FromString(it->second.c_str());
                PyDict_SetItemString(
                    kwargs,
                    it->first.c_str(),
                    temp);
            }

            // Invoke the function
            object_t result = PyObject_Call(function, args, kwargs);
            dict_t dict;

            if(!result.valid() || PyErr_Occurred()) {
                dict["exception"] = exception();
            } else {
                // We got a dict
                if(PyDict_Check(result)) {
                    // Borrowed references, so no need to track them
                    PyObject *key, *value;
                    object_t k(NULL), v(NULL);
                    Py_ssize_t position = 0;

                    // Iterate and convert everything to strings
                    while(PyDict_Next(result, &position, &key, &value)) {
                        k = PyObject_Str(key);
                        v = PyObject_Str(value);
                        
                        dict.insert(std::make_pair(
                            PyString_AsString(k),
                            PyString_AsString(v)));
                    }
                } else if(result != Py_None) {
                    // We got something else, convert it to string and return as-is
                    object_t string = PyObject_Str(result);
                    dict["result"] = PyString_AsString(string);
                }
            }
            
            return dict;
        }

        std::string exception() {
            object_t type(NULL), object(NULL), trackback(NULL);
            PyErr_Fetch(&type, &object, &trackback);

            object_t name = PyObject_Str(type);
            object_t message = PyObject_Str(object);
           
            std::ostringstream result;
            result << PyString_AsString(name)
                   << ": "
                   << PyString_AsString(message);

            PyErr_Clear();
            return result.str();
        }

    private:
        object_t m_code;

        std::string m_path, m_name, m_function;
        dict_t m_args;
};

extern "C" {
    // Source factories
    void* create_instance(const char* uri) {
        return new python_t(uri);
    }

    // Source factories table
    const plugin_info_t info = {
        1,
        {{ "python", &create_instance }}
    };

    // Called by plugin registry on load
    const plugin_info_t* initialize() {
        // Initializes the Python subsystem
        Py_InitializeEx(0);
        
        // Initializes and releases GIL
        PyEval_InitThreads();
        PyEval_ReleaseLock();

        return &info;
    }
}
