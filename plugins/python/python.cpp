#include <Python.h>

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <syslog.h>

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
        
        // Format: python:///path/to/file.py/class?arg1=val1&arg2=...
        python_t(const std::string& uri_):
            m_module(NULL),
            m_object(NULL)
        {
            // Parse the URI
            yappi::helpers::uri_t uri(uri_);
             
            // Get the class name
            std::vector<std::string> url = uri.path();
            std::string classname = url.back();
            url.pop_back();
           
            // Get the file path
            std::vector<std::string>::iterator it = url.begin();
            std::string path("/");

            while(true) {
                path += *it;

                if(++it != url.end())
                    path += '/';
                else
                    break;
            }

            // Try to open the file
            std::ifstream input;
            input.exceptions(std::ifstream::badbit | std::ifstream::failbit);
            
            try {
                input.open(path.c_str(), std::ifstream::in);
            } catch(const std::ifstream::failure& e) {
                throw std::invalid_argument("cannot open " + path);
            }

            // Read the code
            std::stringstream contents;
            contents << input.rdbuf();

            // Get the thread state
            thread_state_t state = PyGILState_Ensure();
            
            // Compile the code
            object_t code = Py_CompileString(
                contents.str().c_str(),
                path.c_str(),
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

            // Check if the class is there
            object_t type = PyObject_GetAttrString(m_module,
                classname.c_str());

            if(PyErr_Occurred()) {
                throw std::runtime_error(exception());
            }

            // And check if it's a type object
            if(!PyType_Check(type)) {
                throw std::runtime_error(classname + " is not a type object");
            }

            // Finalize the type object
            if(PyType_Ready(reinterpret_cast<PyTypeObject*>(*type)) != 0) {
                throw std::runtime_error(exception());
            }

            // Create the instance
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
            
            m_object = PyObject_Call(type, args, kwargs);

            if(PyErr_Occurred()) {
                throw std::runtime_error(exception());
            }

            // And check if it's iterable
            if(!PyIter_Check(m_object)) {
                throw std::runtime_error("object is not iterable");
            }
        }

        virtual dict_t fetch() {
            // Get the thread state
            thread_state_t state = PyGILState_Ensure();

            // Invoke the function
            dict_t dict;
            object_t result = PyIter_Next(m_object);

            if(!result.valid() || PyErr_Occurred()) {
                dict["exception"] = exception();
            } else {
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

            object_t name = PyObject_Str(type);
            object_t message = PyObject_Str(object);
           
            std::ostringstream result;
            result << PyString_AsString(name)
                   << ": "
                   << PyString_AsString(message);

            PyErr_Clear();
            return result.str();
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
}
