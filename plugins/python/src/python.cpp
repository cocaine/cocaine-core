#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <curl/curl.h>

#include "cocaine/config.hpp"
#include "cocaine/helpers/uri.hpp"

#include "python.hpp"
#include "store.hpp"

using namespace cocaine::plugin;

namespace fs = boost::filesystem;

char python_t::identity[] = "<dynamic>";

size_t stream_writer(void* data, size_t size, size_t nmemb, void* stream) {
    std::stringstream* out(reinterpret_cast<std::stringstream*>(stream));
    out->write(reinterpret_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

python_t::python_t(const std::string& uri_):
    source_t(uri_),
    m_module(NULL)
{
    // Parse the URI
    helpers::uri_t uri(uri_);
    
    // Join the path components
    std::vector<std::string> target(uri.path());
    fs::path path(fs::path(config_t::get().registry.location) / "python.d");

    for(std::vector<std::string>::const_iterator it = target.begin(); it != target.end(); ++it) {
        path /= *it;
    }
       
    // Get the code
    std::stringstream code;
    
    if(uri.host().length()) {
        throw std::runtime_error("remote code download feature is disabled");
        
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
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Cocaine/0.5");
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
        fs::ifstream input(path);
        
        if(!input) {
            throw std::runtime_error("failed to open " + path.string());
        }

        // Read the code
        code << input.rdbuf();
    }

    compile(code.str());
}

void python_t::compile(const std::string& code) {
    thread_state_t state(PyGILState_Ensure());
    
    // Compile the code
    object_t bytecode(Py_CompileString(
        code.c_str(),
        identity,
        Py_file_input));

    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    // Execute the code
    m_module = PyImport_ExecCodeModule(
        identity, bytecode);
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    // Instantiate the Store object
#if PY_VERSION_HEX > 0x02070000
    object_t capsule(PyCapsule_New(static_cast<void*>(this), NULL, NULL));
#else
    object_t capsule(PyCObject_FromVoidPtr(static_cast<void*>(this), NULL));
#endif

    object_t args(PyTuple_Pack(1, *capsule));
    
    PyObject* store = PyObject_Call(reinterpret_cast<PyObject*>(&store_object_type),
        args, NULL);
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    // Note: steals the reference    
    PyModule_AddObject(m_module, "cocaine.store", store);    
}

Json::Value python_t::invoke(const std::string& callable, const void* request, size_t request_length) {
    thread_state_t state(PyGILState_Ensure());
    object_t args(NULL);

    if(request) {
        object_t buffer(PyBuffer_FromMemory(const_cast<void*>(request), request_length));
        args = PyTuple_Pack(1, *buffer);
    } else {
        args = PyTuple_New(0);
    }

    object_t object(PyObject_GetAttrString(m_module, callable.c_str()));
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    }

    if(!PyCallable_Check(object)) {
        throw std::runtime_error("'" + callable + "' is not a callable");
    }

    if(PyType_Check(object)) {
        if(PyType_Ready(reinterpret_cast<PyTypeObject*>(*object)) != 0) {
            throw std::runtime_error(exception());
        }
    }

    object_t result(PyObject_Call(object, args, NULL));

    if(PyErr_Occurred()) {
        throw std::runtime_error(exception());
    } else if(result.valid()) {
        return unwrap(result);
    } else {
        return Json::nullValue;
    }
}

std::string python_t::exception() {
    object_t type(NULL), object(NULL), traceback(NULL);
    
    PyErr_Fetch(&type, &object, &traceback);
    object_t message(PyObject_Str(object));
    
    return PyString_AsString(message);
}

Json::Value python_t::unwrap(PyObject* object) {
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
    } else if(PySequence_Check(object) || PyIter_Check(object)) {
        object_t iterator(PyObject_GetIter(object));
        object_t item(NULL);

        while(item = PyIter_Next(iterator)) {
            result.append(unwrap(item));
        }
    } else if(object != Py_None) {
        result = "<error: unrecognized type>";
    }    

    return result;
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

        // Initialize the storage type object
        if(PyType_Ready(&store_object_type) < 0) {
            return NULL;
        }
        
        PyEval_ReleaseLock();

        return plugin_info;
    }

    __attribute__((destructor)) void finalize() {
        // XXX: This SEGFAULTs the process for some reason during the finalization
        // Py_Finalize();
    }
}
