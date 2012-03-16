//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
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

#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>
#include <sstream>

#include "python.hpp"
#include "objects.hpp"

#include "cocaine/app.hpp"
#include "cocaine/registry.hpp"

using namespace cocaine;
using namespace cocaine::core;
using namespace cocaine::engine;

static PyMethodDef context_module_methods[] = {
    { "manifest", &python_t::manifest, METH_NOARGS, 
        "Get the application's manifest" },
    { NULL, NULL, 0, NULL }
};

python_t::python_t(context_t& ctx):
    plugin_t(ctx),
    m_python_module(NULL),
    m_manifest(NULL)
{
    Py_InitializeEx(0);
    PyEval_InitThreads();

    // Initializing types.
    PyType_Ready(&log_object_type);
    PyType_Ready(&python_io_object_type);
}

python_t::~python_t() {
    PyEval_RestoreThread(m_thread_state); 
    Py_Finalize();
}

void python_t::initialize(const app_t& app) {
    m_app_log = app.log;

    Json::Value args(app.manifest["args"]);

    if(!args.isObject()) {
        throw configuration_error_t("malformed manifest");
    }
    
    boost::filesystem::path source(args["source"].asString());
   
    if(source.empty()) {
        throw configuration_error_t("no code location has been specified");
    }

    // NOTE: Means it's a module.
    if(boost::filesystem::is_directory(source)) {
        source /= "__init__.py";
    }

    m_app_log->debug("loading the app code from %s", source.string().c_str());
    
    boost::filesystem::ifstream input(source);
    
    if(!input) {
        throw configuration_error_t("unable to open " + source.string());
    }

    // System paths
    // ------------

    static char key[] = "path";

    // NOTE: Prepend the current application location to the sys.path,
    // so that it could import various local stuff from there.
    PyObject * syspaths = PySys_GetObject(key);
    
    tracked_object_t path(
        PyString_FromString(
#if BOOST_FILESYSTEM_VERSION == 3
            source.parent_path().string().c_str()
#else
            source.branch_path().string().c_str()
#endif
        )
    );

    PyList_Insert(syspaths, 0, path);

    // Context access module
    // ---------------------

    m_manifest = wrap(args);

    PyObject * context_module = Py_InitModule(
        "__context__",
        context_module_methods
    );

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

    PyObject * builtins = PyEval_GetBuiltins();

    tracked_object_t plugin(
        PyCObject_FromVoidPtr(this, NULL)
    );

    PyDict_SetItemString(
        builtins,
        "__plugin__",
        plugin
    );

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

    // Code evaluation
    // ---------------

    std::stringstream stream;
    stream << input.rdbuf();

    tracked_object_t bytecode(
        Py_CompileString(
            stream.str().c_str(),
            source.string().c_str(),
            Py_file_input
        )
    );

    if(PyErr_Occurred()) {
        throw unrecoverable_error_t(exception());
    }

    PyObject * globals = PyModule_GetDict(m_python_module);
    
    // NOTE: This will return None or NULL due to the Py_file_input flag above,
    // so we can safely drop it without even checking.
    tracked_object_t result(
        PyEval_EvalCode(
            reinterpret_cast<PyCodeObject*>(*bytecode), 
            globals,
            NULL
        )
    );
    
    if(PyErr_Occurred()) {
        throw unrecoverable_error_t(exception());
    }

    m_thread_state = PyEval_SaveThread();
}

void python_t::invoke(io_t& io, const std::string& method) {
    thread_lock_t thread(m_thread_state);

    if(!m_python_module) {
        throw unrecoverable_error_t("python module is not initialized");
    }

    PyObject * globals = PyModule_GetDict(m_python_module);
    PyObject * object = PyDict_GetItemString(globals, method.c_str());
    
    if(PyErr_Occurred()) {
        throw unrecoverable_error_t(exception());
    }

    if(PyType_Check(object)) {
        if(PyType_Ready(reinterpret_cast<PyTypeObject*>(object)) != 0) {
            throw unrecoverable_error_t(exception());
        }
    }

    if(!PyCallable_Check(object)) {
        throw unrecoverable_error_t("'" + method + "' is not callable");
    }

    tracked_object_t args(NULL);

    // Passing io_t object to the python io_t wrapper.
    tracked_object_t io_object(
        PyCObject_FromVoidPtr(&io, NULL)
    );

    args = PyTuple_Pack(1, *io_object);

    tracked_object_t io_proxy(
        PyObject_Call(
            reinterpret_cast<PyObject*>(&python_io_object_type), 
            args,
            NULL
        )
    );

    args = PyTuple_Pack(1, *io_proxy);

    tracked_object_t result(PyObject_Call(object, args, NULL));

    if(PyErr_Occurred()) {
        throw recoverable_error_t(exception());
    } 
   
    if(result != Py_None) {
        m_app_log->warning(
            "ignoring an unused returned value of method '%s'",
            method.c_str()
        );
    }
    
    // Commented out due to the python io_t wrapper.
    // else if(result.valid() && result != Py_None) {
    //     respond(io, result);
    // }
}

const logging::logger_t& python_t::log() const {
    return *m_app_log;
}

PyObject* python_t::manifest(PyObject * self, PyObject * args) {
    PyObject * builtins = PyEval_GetBuiltins();
    PyObject * plugin = PyDict_GetItemString(builtins, "__plugin__");

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

// XXX: Check reference counting.
PyObject* python_t::wrap(const Json::Value& value) {
    PyObject * object = NULL;

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

std::string python_t::exception() {
    tracked_object_t type(NULL), value(NULL), traceback(NULL);
    
    PyErr_Fetch(&type, &value, &traceback);

    tracked_object_t name(PyObject_Str(type));
    tracked_object_t message(PyObject_Str(value));
    
    boost::format formatter("uncaught exception %s: %s");
    
    std::string result(
        (formatter
            % PyString_AsString(name)
            % PyString_AsString(message)
        ).str()
    );

    return result;
}

extern "C" {
    void initialize(registry_t& registry) {
        registry.install<python_t, plugin_t>("python");
    }
}
