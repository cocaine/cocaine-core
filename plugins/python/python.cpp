#include "python.hpp"

#include <stdexcept>
#include <fstream>

#include <curl/curl.h>

#include "uri.hpp"

namespace yappi { namespace plugin {

char python_t::identity[] = "yappi-dynamic";

size_t stream_writer(void* data, size_t size, size_t nmemb, void* stream) {
    std::stringstream* out = reinterpret_cast<std::stringstream*>(stream);
    out->write(reinterpret_cast<char*>(data), size * nmemb);

    return size * nmemb;
}

python_t::python_t(const std::string& uri_):
    source_t(uri_),
    m_module(NULL),
    m_object(NULL)
{
    // Parse the URI
    helpers::uri_t uri(uri_);
    
    // Get the callable name
    std::vector<std::string> target = uri.path();
    std::string name = target.back();
    target.pop_back();

    // Join the path components
    std::string path("/usr/lib/yappi/python.d");
    std::vector<std::string>::const_iterator it = target.begin();
       
    do {
        path += ('/' + *it);
        ++it;
    } while(it != target.end());
    
    // Get the code
    std::stringstream code;
    
    if(uri.host().length()) {
        throw std::runtime_error("remote code download feature disabled");
        
        /*
        char error_message[CURL_ERROR_SIZE];
        CURL* curl = curl_easy_init();

        if(!curl) {
            throw std::runtime_error("failed to initialize libcurl");
        }

        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_message);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &stream_writer);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &code);
        curl_easy_setopt(curl, CURLOPT_URL, ("http://" + uri.host() + path).c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Yappi/0.1");
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1000);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

        if(curl_easy_perform(curl) != 0) {
            throw std::runtime_error(error_message);
        }

        curl_easy_cleanup(curl);
        */
    } else {
        // The code is stored locally
        std::ifstream input(path.c_str());
        
        if(!input.is_open()) {
            throw std::runtime_error("cannot open " + path);
        }

        // Read the code
        code << input.rdbuf();
    }

    compile(code.str(), name, uri.query());
}

void python_t::compile(const std::string& code,
                       const std::string& name,
                       const dict_t& parameters)
{
    // Get the thread state
    thread_state_t state = PyGILState_Ensure();
    
    // Compile the code
    object_t bytecode = Py_CompileString(
        code.c_str(),
        "<dynamic>",
        Py_file_input);

    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    // Execute the code
    m_module = PyImport_ExecCodeModule(
        identity, bytecode);
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    // Check if the callable is there
    object_t callable = PyObject_GetAttrString(m_module,
        name.c_str());

    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    // And check if it's, well, callable
    if(!PyCallable_Check(callable)) {
        throw std::runtime_error(name + " is not callable");
    }

    // If it's a type object, finalize it
    if(PyType_Check(callable)) {
        if(PyType_Ready(reinterpret_cast<PyTypeObject*>(*callable)) != 0) {
            throw std::runtime_error(exception());
        }
    }

    // Call it to create an instance
    object_t args = PyTuple_New(0);
    object_t kwargs = PyDict_New();

    for(dict_t::const_iterator it = parameters.begin(); it != parameters.end(); ++it) {
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
        throw std::runtime_error("object is not iterable");
    }
}

dict_t python_t::invoke() {
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
    } else {
        throw exhausted("iteration stopped");
    }
    
    return dict;
}

float python_t::reschedule() {
    thread_state_t state = PyGILState_Ensure();
    object_t reschedule = PyObject_GetAttrString(m_object, "reschedule");
    
    object_t args = PyTuple_New(0);
    object_t result = PyObject_Call(reschedule, args, NULL);
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    if(!PyFloat_Check(result)) {
        throw std::runtime_error("reschedule() has returned a non-float object");
    }

    return PyFloat_AsDouble(result);
}

uint32_t python_t::capabilities() const {
    thread_state_t state = PyGILState_Ensure();
    object_t reschedule = PyObject_GetAttrString(m_object, "reschedule");

    return SINK | (PyCallable_Check(reschedule) ? MANUAL : NONE);
}

std::string python_t::exception() {
    object_t type(NULL), object(NULL), trackback(NULL);
    
    PyErr_Fetch(&type, &object, &trackback);
    object_t message = PyObject_Str(object);
    PyErr_Clear();
    
    return PyString_AsString(message);
}

source_t* create_python_instance(const char* uri) {
    return new python_t(uri);
}

const plugin_info_t plugin_info = {
    1,
    {
        { "python", &create_python_instance }
    }
};

static char* argv[] = { python_t::identity };

extern "C" {
    const plugin_info_t* initialize() {
        // Initializes the Python subsystem
        Py_InitializeEx(0);

        // Set the argc/argv in sys module
        PySys_SetArgv(1, argv);
        
        // Initializes and releases GIL
        PyEval_InitThreads();
        PyEval_ReleaseLock();

        return &plugin_info;
    }

    __attribute__((destructor)) void finalize() {
        // XXX: This SEGFAULTs the process for some reason during the finalization
        // Py_Finalize();
    }
}

}}
