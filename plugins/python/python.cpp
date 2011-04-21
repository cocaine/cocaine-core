#include <Python.h>

#include <fstream>
#include <stdexcept>

#include "plugin.hpp"
#include "uri.hpp"
#include "track.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error
// * std::invalid_argument

using namespace yappi::plugin;
using namespace yandex::helpers;

class python_t: public source_t {
    public:
        typedef track<PyGILState_STATE, PyGILState_Release> thread_state_t;
        typedef track<PyObject*, Py_DecRef> object_t;
        
        // Format: python:///path/to/file.py/callable?arg1=val1&arg2=...
        python_t(const std::string& uri_):
            m_module(NULL),
            m_object(NULL)
        {
            // Parse the URI
            yappi::helpers::uri_t uri(uri_);
             
            // Get the callable name
            std::vector<std::string> path = uri.path();
            
            std::string name = path.back();
            path.pop_back();
           
            // Get the code location
            std::vector<std::string>::iterator it = path.begin();
            std::string location("/");

            while(true) {
                location += *it;

                if(++it != path.end())
                    location += '/';
                else
                    break;
            }

            // Try to open the file
            std::ifstream input;
            input.exceptions(std::ifstream::badbit | std::ifstream::failbit);
            
            try {
                input.open(location.c_str(), std::ifstream::in);
            } catch(const std::ifstream::failure& e) {
                throw std::invalid_argument("cannot open " + location);
            }

            // Read the code
            std::stringstream contents;
            contents << input.rdbuf();

            // Get the thread state
            thread_state_t state = PyGILState_Ensure();
            
            // Compile the code
            object_t code = Py_CompileString(
                contents.str().c_str(),
                location.c_str(),
                Py_file_input);

            if(PyErr_Occurred()) {
                throw std::runtime_error(exception());
            }

            // Execute the code
            m_module = PyImport_ExecCodeModule(
                module_name, code);
            
            if(PyErr_Occurred()) {
                throw std::runtime_error(exception());
            }

            // Check if the callable is there
            object_t callable = PyObject_GetAttrString(m_module,
                name.c_str());

            if(PyErr_Occurred()) {
                throw std::invalid_argument(exception());
            }

            // And check if it's, well, callable
            if(!PyCallable_Check(callable)) {
                throw std::invalid_argument(name + " is not callable");
            }

            // If it's a type object, finalize it
            if(PyType_Check(callable)) {
                if(PyType_Ready(reinterpret_cast<PyTypeObject*>(*callable)) != 0) {
                    throw std::runtime_error(exception());
                }
            }

            // Call it to create an instance
            dict_t parameters = uri.query();

            object_t args = PyTuple_New(0);
            object_t kwargs = PyDict_New();

            for(dict_t::iterator it = parameters.begin(); it != parameters.end(); ++it) {
                object_t temp = PyString_FromString(it->second.c_str());
                
                PyDict_SetItemString(
                    kwargs,
                    it->first.c_str(),
                    temp);
            }
            
            m_object = PyObject_Call(callable, args, kwargs);

            if(PyErr_Occurred()) {
                throw std::runtime_error(exception());
            }

            // And check if the instance is iterable, i.e. a generator
            // or an iterable object
            if(!PyIter_Check(m_object)) {
                throw std::invalid_argument("object is not iterable");
            }
        }

        virtual dict_t fetch() {
            // Get the thread state
            thread_state_t state = PyGILState_Ensure();

            // Invoke the function
            dict_t dict;
            object_t result = PyIter_Next(m_object);

            if(PyErr_Occurred()) {
                dict["exception"] = exception();
            } else if(result.valid()) {
                if(PyDict_Check(result)) {
                    // Borrowed references, so no need to track them
                    PyObject *key, *value;
                    Py_ssize_t position = 0;
                    
                    object_t k(NULL), v(NULL);

                    // Iterate and convert everything to strings
                    while(PyDict_Next(result, &position, &key, &value)) {
                        k = PyObject_Str(key);
                        v = PyObject_Str(value);
                        
                        dict.insert(std::make_pair(
                            PyString_AsString(k),
                            PyString_AsString(v)));
                    }
                } else if(result != Py_None) {
                    // Convert it to string and return as-is
                    object_t string = PyObject_Str(result);
                    dict["result"] = PyString_AsString(string);
                }
            }
            
            return dict;
        }

        std::string exception() {
            object_t type(NULL), object(NULL), trackback(NULL);
            PyErr_Fetch(&type, &object, &trackback);

            object_t message = PyObject_Str(object);
           
            PyErr_Clear();
            
            return PyString_AsString(message);
        }

    protected:
        object_t m_module, m_object;
        static char module_name[];
};

char python_t::module_name[] = "yappi";

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

    __attribute__((destructor)) void finalize() {
        Py_Finalize();
    }
}
