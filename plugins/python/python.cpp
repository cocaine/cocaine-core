//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <sstream>
#include <boost/filesystem/fstream.hpp>

#include "python.hpp"
#include "log.hpp"

#include "cocaine/app.hpp"
#include "cocaine/registry.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;

static char sys_path_name[] = "path";

static PyMethodDef context_module_methods[] = {
    { "manifest", &python_t::manifest, METH_NOARGS, "Get the application's manifest" },
    { NULL, NULL, 0, NULL }
};

python_t::python_t(context_t& ctx):
    plugin_t(ctx, "python"),
    m_python_module(NULL),
    m_manifest(NULL)
{ }

void python_t::initialize(const app_t& app) {
    Json::Value args(app.manifest["args"]);

    if(!args.isObject()) {
        throw unrecoverable_error_t("malformed manifest");
    }
    
    boost::filesystem::path source(args["source"].asString());
   
    if(source.empty()) {
        throw unrecoverable_error_t("no code location has been specified");
    }

    // NOTE: Means it's a module.
    if(boost::filesystem::is_directory(source)) {
        source /= "__init__.py";
    }

    boost::filesystem::ifstream input(source);
    
    if(!input) {
        throw unrecoverable_error_t("unable to open " + source.string());
    }

    // Acquire the interpreter state.
    thread_state_t state;

    // System paths
    // ------------

    // NOTE: Prepend the current application location to the sys.path,
    // so that it could import various local stuff from there.
    PyObject* syspaths = PySys_GetObject(sys_path_name);
    
    python_object_t path(
        PyString_FromString(
#if BOOST_FILESYSTEM_VERSION == 3
            source.parent_path().string().c_str()
#else
            source.branch_path().string().c_str()
#endif
        )
    );

    PyList_Insert(syspaths, 0, path);

    // Application context
    // -------------------

    m_manifest = wrap(args);

    PyObject* context_module = Py_InitModule(
        "__context__",
        context_module_methods
    );

    PyType_Ready(&log_object_type);
    Py_INCREF(&log_object_type);
    
    PyModule_AddObject(
        context_module,
        "Log",
        reinterpret_cast<PyObject*>(&log_object_type)
    );

    // Application module
    // ------------------

    m_python_module = Py_InitModule(
        app.name.c_str(),
        NULL
    );

    PyObject* builtins = PyEval_GetBuiltins();
    Py_INCREF(builtins);

    PyModule_AddObject(
        m_python_module, 
        "__builtins__",
        builtins
    );
    
    PyModule_AddStringConstant(
        m_python_module,
        "__file__",
        source.string().c_str()
    );

    PyModule_AddObject(
        m_python_module,
        "__plugin__",
        PyCObject_FromVoidPtr(this, NULL)
    );

    // Code evaluation
    // ---------------

    std::stringstream stream;
    stream << input.rdbuf();

    python_object_t bytecode(
        Py_CompileString(
            stream.str().c_str(),
            source.string().c_str(),
            Py_file_input
        )
    );

    if(PyErr_Occurred()) {
        throw unrecoverable_error_t(exception());
    }

    python_object_t globals(PyModule_GetDict(m_python_module));
    
    // NOTE: This will return None or NULL due to the Py_file_input flag above,
    // so we can safely drop it without even checking.
    python_object_t result(
        PyEval_EvalCode(
            reinterpret_cast<PyCodeObject*>(*bytecode), 
            globals,
            NULL
        )
    );
    
    if(PyErr_Occurred()) {
        throw unrecoverable_error_t(exception());
    }
}

void python_t::invoke(io_t& io, const std::string& method) {
    thread_state_t state;
    
    if(!m_python_module) {
        throw unrecoverable_error_t("python module is not initialized");
    }

    python_object_t object(
        PyObject_GetAttrString(
            m_python_module,
            method.c_str()
        )
    );
    
    if(PyErr_Occurred()) {
        throw unrecoverable_error_t(exception());
    }

    if(PyType_Check(object)) {
        if(PyType_Ready(reinterpret_cast<PyTypeObject*>(*object)) != 0) {
            throw unrecoverable_error_t(exception());
        }
    }

    if(!PyCallable_Check(object)) {
        throw unrecoverable_error_t("'" + method + "' is not callable");
    }

    python_object_t args(NULL);
#if PY_VERSION_HEX >= 0x02070000
    boost::shared_ptr<Py_buffer> buffer;
#endif

    // NOTE: It's safe to const_cast() the request buffer, as both of the used
    // representation methods expose the buffer as a read-only object to the user code.

    if(io.request && io.request_size) {
#if PY_VERSION_HEX >= 0x02070000
        buffer.reset(static_cast<Py_buffer*>(malloc(sizeof(Py_buffer))), free);

        buffer->buf = const_cast<void*>(io.request);
        buffer->len = io.request_size;
        buffer->readonly = true;
        buffer->format = NULL;
        buffer->ndim = 0;
        buffer->shape = NULL;
        buffer->strides = NULL;
        buffer->suboffsets = NULL;

        python_object_t view(PyMemoryView_FromBuffer(buffer.get()));
#else
        python_object_t view(PyBuffer_FromMemory(
            const_cast<void*>(io.request), 
            io.request_size));
#endif

        args = PyTuple_Pack(1, *view);
    } else {
        args = PyTuple_New(0);
    }

    python_object_t result(PyObject_Call(object, args, NULL));

    if(PyErr_Occurred()) {
        throw recoverable_error_t(exception());
    } else if(result.valid()) {
        respond(io, result);
    }
}

PyObject* python_t::manifest(PyObject* self, PyObject*) {
    PyObject* globals(PyEval_GetGlobals());
    PyObject* plugin(PyDict_GetItemString(globals, "__plugin__"));

    if(!plugin) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Corrupted context"
        );

        return NULL;
    }

    return PyDictProxy_New(
        static_cast<python_t*>(PyCObject_AsVoidPtr(plugin))->m_manifest
    );
}

std::string python_t::exception() {
    python_object_t type(NULL), object(NULL), traceback(NULL);
    
    PyErr_Fetch(&type, &object, &traceback);
    python_object_t message(PyObject_Str(object));
    
    return PyString_AsString(message);
}

void python_t::respond(io_t& io, python_object_t& result) {
    if(PyString_Check(result) || !PyIter_Check(result)) {
        throw recoverable_error_t("the result must be an iterable");
    }

    python_object_t iterator(PyObject_GetIter(result));

    if(iterator.valid()) {
        python_object_t item(NULL);

        while(true) {
            item = PyIter_Next(iterator);

            if(PyErr_Occurred()) {
                throw recoverable_error_t(exception());
            } else if(!item.valid()) {
                break;
            }
        
#if PY_VERSION_HEX >= 0x02060000
            if(PyObject_CheckBuffer(item)) {
                boost::shared_ptr<Py_buffer> buffer(
                    static_cast<Py_buffer*>(malloc(sizeof(Py_buffer))),
                    free
                );

                if(PyObject_GetBuffer(item, buffer.get(), PyBUF_SIMPLE) == 0) {
                    Py_BEGIN_ALLOW_THREADS
                        io.push(buffer->buf, buffer->len);
                    Py_END_ALLOW_THREADS
                    
                    PyBuffer_Release(buffer.get());
                } else {
                    throw recoverable_error_t("unable to serialize the result");
                }
            }
#else
            if(PyString_Check(item)) {
                io.push(PyString_AsString(item), PyString_Size(item));
            } else {
                throw recoverable_error_t("unable to serialize the result");
            }
#endif
        }
    } else {
        throw recoverable_error_t(exception());
    }
}

// XXX: Check reference counting.
PyObject* python_t::wrap(const Json::Value& value) {
    PyObject* object = NULL;

    switch(value.type()) {
        case Json::booleanValue:
            return PyBool_FromLong(value.asBool());
        case Json::intValue:
        case Json::uintValue:
            return PyLong_FromLong(value.asInt());
        case Json::realValue:
            return PyFloat_FromDouble(value.asDouble());
        case Json::stringValue:
            return PyString_FromString(value.asCString());
        case Json::objectValue: {
            object = PyDict_New();
            Json::Value::Members names(value.getMemberNames());

            for(Json::Value::Members::iterator it = names.begin();
                it != names.end();
                ++it) 
            {
                PyDict_SetItemString(object, it->c_str(), wrap(value[*it]));
            }

            break;
        } case Json::arrayValue: {
            object = PyTuple_New(value.size());
            Py_ssize_t position = 0;

            for(Json::Value::const_iterator it = value.begin(); 
                it != value.end();
                ++it) 
            {
                PyTuple_SetItem(object, position++, wrap(*it));
            }

            break;
        } case Json::nullValue:
            Py_RETURN_NONE;
    }

    return object;
}

PyThreadState* g_state = NULL;

void save() {
    g_state = PyEval_SaveThread();
}

void restore() {
    PyEval_RestoreThread(g_state);
}

extern "C" {
    void initialize(registry_t& registry) {
        // Initialize the Python subsystem.
        Py_InitializeEx(0);

        // Initialize the GIL.
        PyEval_InitThreads();

        // Save the main thread.
        save();

        // NOTE: In case of a fork, restore the main thread state and acquire the GIL,
        // call the python post-fork handler and save the main thread again, releasing the GIL.
        pthread_atfork(NULL, NULL, restore);
        pthread_atfork(NULL, NULL, PyOS_AfterFork);
        pthread_atfork(NULL, NULL, save);

        registry.install("python", &python_t::create);
    }

    __attribute__((destructor)) void finalize() {
        restore();
        Py_Finalize();
    }
}
