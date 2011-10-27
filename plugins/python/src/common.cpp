#include "cocaine/downloads.hpp"
#include "cocaine/helpers/uri.hpp"

#include "common.hpp"
#include "python+wsgi.hpp"
#include "python+json.hpp"

using namespace cocaine::plugin;

char python_t::identity[] = "<dynamic>";

void python_t::exception() {
    object_t type(NULL), object(NULL), traceback(NULL);
    
    PyErr_Fetch(&type, &object, &traceback);
    object_t message(PyObject_Str(object));
    
    throw std::runtime_error(PyString_AsString(message));
}

python_t::python_t(const std::string& args):
    m_module(NULL)
{
    if(args.empty()) {
        throw std::runtime_error("no code location has been specified");
    }

    helpers::uri_t uri(args);
    helpers::download_t app(helpers::download(uri));   

    thread_state_t state(PyGILState_Ensure());
    
    // NOTE: Prepend the current application cache location to the sys.path,
    // so that it could import different stuff from there
    PyObject* paths = PySys_GetObject("path");

    if(PyList_Check(paths)) {
        // XXX: Does it steal the reference or not?
        PyList_Insert(paths, 0, 
            PyString_FromString(app.path().string().c_str()));
        compile(app);
    } else {
        throw std::runtime_error("sys.path is not a list");
    }
}

void python_t::compile(const std::string& code) {
    object_t bytecode(Py_CompileString(
        code.c_str(),
        identity,
        Py_file_input));

    if(PyErr_Occurred()) {
        exception();
    }

    m_module = PyImport_ExecCodeModule(
        identity, bytecode);
    
    if(PyErr_Occurred()) {
        exception();
    }
}

void python_t::invoke(
    callback_fn_t callback,
    const std::string& method,
    const void* request,
    size_t size) 
{
    thread_state_t state(PyGILState_Ensure());
    object_t args(NULL);

    if(request) {
#if PY_VERSION_HEX > 0x02070000
        Py_buffer* buffer = static_cast<Py_buffer*>(malloc(sizeof(Py_buffer)));

        buffer->buf = const_cast<void*>(request);
        buffer->len = size;
        buffer->readonly = true;
        buffer->format = NULL;
        buffer->ndim = 0;
        buffer->shape = NULL;
        buffer->strides = NULL;
        buffer->suboffsets = NULL;

        object_t view(PyMemoryView_FromBuffer(buffer));
#else
        object_t view(PyBuffer_FromMemory(const_cast<void*>(request), size));
#endif

        args = PyTuple_Pack(1, *view);
    } else {
        args = PyTuple_New(0);
    }

    object_t object(PyObject_GetAttrString(m_module, method.c_str()));
    
    if(PyErr_Occurred()) {
        exception();
    }

    if(!PyCallable_Check(object)) {
        throw std::runtime_error("'" + method + "' is not callable");
    }

    if(PyType_Check(object)) {
        if(PyType_Ready(reinterpret_cast<PyTypeObject*>(*object)) != 0) {
            exception();
        }
    }

    object_t result(PyObject_Call(object, args, NULL));

    if(result.valid()) {
        respond(callback, result);
    } else if(PyErr_Occurred()) {
        exception();
    }
}

static const source_info_t plugin_info[] = {
    { "python+wsgi", &python_wsgi_t::create },
    { "python+json", &python_json_t::create },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        // Initialize the Python subsystem
        Py_InitializeEx(0);

        // Initialize the GIL
        PyEval_InitThreads();
        PyEval_ReleaseLock();

        pthread_atfork(NULL, NULL, PyOS_AfterFork);

        return plugin_info;
    }

    __attribute__((destructor)) void finalize() {
        Py_Finalize();
    }
}
