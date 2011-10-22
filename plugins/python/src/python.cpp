#include "cocaine/downloads.hpp"
#include "cocaine/helpers/uri.hpp"

#include "python.hpp"
#include "storage.hpp"

using namespace cocaine::plugin;

char python_t::identity[] = "<dynamic>";

source_t* python_t::create(const std::string& name, const std::string& args) {
    return new python_t(name, args);
}

python_t::python_t(const std::string& name, const std::string& args):
    source_t(name),
    m_module(NULL)
{
    if(args.empty()) {
        throw std::runtime_error("no code location has been specified");
    }

    helpers::uri_t uri(args);
    compile(helpers::download(uri));
}

void python_t::compile(const std::string& code) {
    thread_state_t state(PyGILState_Ensure());
    
    // Compile the code
    object_t bytecode(Py_CompileString(
        code.c_str(),
        identity,
        Py_file_input));

    if(PyErr_Occurred()) {
        throw std::runtime_error(python_support_t::exception());
    }

    // Execute the code
    m_module = PyImport_ExecCodeModule(
        identity, bytecode);
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(python_support_t::exception());
    }

    // Instantiate the Store object
#if PY_VERSION_HEX > 0x02070000
    object_t capsule(PyCapsule_New(static_cast<void*>(this), NULL, NULL));
#else
    object_t capsule(PyCObject_FromVoidPtr(static_cast<void*>(this), NULL));
#endif

    object_t args(PyTuple_Pack(1, *capsule));
    
    PyObject* storage = PyObject_Call(reinterpret_cast<PyObject*>(&storage_object_type),
        args, NULL);
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(python_support_t::exception());
    }

    // Note: steals the reference
    PyModule_AddObject(m_module, "storage", storage);    
}

Json::Value python_t::invoke(const std::string& method, const void* request, size_t request_size) {
    thread_state_t state(PyGILState_Ensure());
    object_t args(NULL);

    if(request) {
        object_t buffer(PyBuffer_FromMemory(const_cast<void*>(request), request_size));
        args = PyTuple_Pack(1, *buffer);
    } else {
        args = PyTuple_New(0);
    }

    object_t object(PyObject_GetAttrString(m_module, method.c_str()));
    
    if(PyErr_Occurred()) {
        throw std::runtime_error(python_support_t::exception());
    }

    if(!PyCallable_Check(object)) {
        throw std::runtime_error("'" + method + "' is not callable");
    }

    if(PyType_Check(object)) {
        if(PyType_Ready(reinterpret_cast<PyTypeObject*>(*object)) != 0) {
            throw std::runtime_error(python_support_t::exception());
        }
    }

    object_t result(PyObject_Call(object, args, NULL));

    if(PyErr_Occurred()) {
        throw std::runtime_error(python_support_t::exception());
    } else if(result.valid()) {
        return python_support_t::unwrap(result);
    } else {
        return Json::nullValue;
    }
}

static const source_info_t plugin_info[] = {
    { "python", &python_t::create },
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
        if(PyType_Ready(&storage_object_type) < 0) {
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
