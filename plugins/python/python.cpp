#include <stdexcept>
#include <fstream>

#include <curl/curl.h>

#include "python.hpp"
#include "store.hpp"

#include "uri.hpp"

using namespace yappi::plugin;

char python_t::identity[] = "<dynamic>";

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
    thread_state_t state(PyGILState_Ensure());
    
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

    // Instantiate the Store object
#if PY_VERSION_HEX > 0x02070000
    object_t capsule = PyCapsule_New(static_cast<void*>(this), NULL, NULL);
#else
    object_t capsule = PyCObject_FromVoidPtr(static_cast<void*>(this), NULL);
#endif

    object_t args = PyTuple_Pack(1, *capsule);
    object_t kwargs = PyDict_New();

    PyObject* store = PyObject_Call(reinterpret_cast<PyObject*>(&store_object_type),
        args, kwargs);
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    // Note: steals the reference    
    PyModule_AddObject(m_module, "store", store);    
    
    // Create the user code object instance
    for(dict_t::const_iterator it = parameters.begin(); it != parameters.end(); ++it) {
        object_t temp = PyString_FromString(it->second.c_str());
        
        PyDict_SetItemString(
            kwargs,
            it->first.c_str(),
            temp);
    }
    
    object_t empty_args = PyTuple_New(0);
    m_object = PyObject_Call(callable, empty_args, kwargs);

    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }
}

uint32_t python_t::capabilities() const {
    thread_state_t state(PyGILState_Ensure());

    object_t reschedule = PyObject_GetAttrString(m_object, "reschedule");
    object_t process = PyObject_GetAttrString(m_object, "process");

    return NONE |
        (PyIter_Check(*m_object) ? ITERATOR : NONE) |
        (PyCallable_Check(reschedule) ? SCHEDULER : NONE) |
        (PyCallable_Check(process) ? PROCESSOR : NONE);
}

dict_t python_t::invoke() {
    // Get the thread state
    thread_state_t state(PyGILState_Ensure());

    // Invoke the function
    object_t result = PyIter_Next(m_object);
    dict_t dict;

    if(PyErr_Occurred()) {
        dict["exception"] = exception();
    } else if(result.valid()) {
        dict = unwrap(result);
    } else {
        throw exhausted("iteration stopped");
    }
    
    return dict;
}

float python_t::reschedule() {
    thread_state_t state(PyGILState_Ensure());
    
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

dict_t python_t::process(const void* data, size_t data_size) {
    thread_state_t state(PyGILState_Ensure());
    
    // This creates a read-only buffer, so it's safe to const_cast
    object_t buffer = PyBuffer_FromMemory(const_cast<void*>(data), data_size);
    object_t process = PyObject_GetAttrString(m_object, "process");

    object_t args = PyTuple_Pack(1, *buffer);
    object_t result = PyObject_Call(process, args, NULL);

    if(PyErr_Occurred()) {
        dict_t dict;
        dict["exception"] = exception();
        return dict;
    }

    return unwrap(result);
}

std::string python_t::exception() {
    object_t type(NULL), object(NULL), trackback(NULL);
    
    PyErr_Fetch(&type, &object, &trackback);
    object_t message = PyObject_Str(object);
    PyErr_Clear();
    
    return PyString_AsString(message);
}

dict_t python_t::unwrap(object_t& object) {
    dict_t dict;
    
    if(PyDict_Check(object)) {
        // Borrowed references, so no need to track them
        PyObject *key, *value;
        Py_ssize_t position = 0;
        
        object_t k(NULL), v(NULL);

        // Iterate and convert everything to strings
        while(PyDict_Next(object, &position, &key, &value)) {
            k = PyObject_Str(key);
            v = PyObject_Str(value);
            
            dict.insert(std::make_pair(
                PyString_AsString(k),
                PyString_AsString(v)));
        }
    } else if(object != Py_None) {
        // Convert it to string and return as-is
        object_t string = PyObject_Str(object);
        dict["result"] = PyString_AsString(string);
    }

    return dict;
}

source_t* create_python_instance(const char* uri) {
    return new python_t(uri);
}

static const source_info_t plugin_info[] = {
    { "python", &create_python_instance },
    { NULL, NULL }
};

static char* argv[] = { python_t::identity };

extern "C" {
    const source_info_t* initialize() {
        // Initialize the Python subsystem
        Py_InitializeEx(0);

        // Set the argc/argv in sys module
        PySys_SetArgv(1, argv);

        // Initialize the GIL
        PyEval_InitThreads();
        PyEval_ReleaseLock();

        // Initialize the storage type object
        if(PyType_Ready(&store_object_type) < 0) {
            return NULL;
        }

        return plugin_info;
    }

    __attribute__((destructor)) void finalize() {
        // XXX: This SEGFAULTs the process for some reason during the finalization
        // Py_Finalize();
    }
}
