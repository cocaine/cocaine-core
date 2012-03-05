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

#include "log.hpp"
#include "python.hpp"

using namespace cocaine::engine;

int log_object_t::constructor(log_object_t* self, PyObject* args, PyObject* kwargs) {
    PyObject* globals = PyEval_GetGlobals();
    PyObject* plugin = PyDict_GetItemString(globals, "__plugin__");
    
    if(plugin) {
        self->plugin = static_cast<python_t*>(PyCObject_AsVoidPtr(plugin));
    } else {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Corrupted context"
        );

        return -1;
    }

    return 0;
}

void log_object_t::destructor(log_object_t* self) {
    self->ob_type->tp_free(self);
}

PyObject* log_object_t::debug(log_object_t* self, PyObject* args) {
    PyObject* object;
    const char* message;

    if(!self->plugin) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Not initialized"
        );

        return NULL;
    }

    if(!PyArg_ParseTuple(args, "O:debug", &object)) {
        return NULL;
    }

    if(!PyString_Check(object)) {
        python_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    self->plugin->log().debug("%s", message);

    Py_RETURN_NONE;
}

PyObject* log_object_t::info(log_object_t* self, PyObject* args) {
    PyObject* object;
    const char* message;

    if(!self->plugin) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Not initialized"
        );

        return NULL;
    }
    
    if(!PyArg_ParseTuple(args, "O:info", &object)) {
        return NULL;
    }

    if(!PyString_Check(object)) {
        python_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    self->plugin->log().info("%s", message);

    Py_RETURN_NONE;
}

PyObject* log_object_t::warning(log_object_t* self, PyObject* args) {
    PyObject* object;
    const char* message;

    if(!self->plugin) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Not initialized"
        );

        return NULL;
    }
    
    if(!PyArg_ParseTuple(args, "O:warning", &object)) {
        return NULL;
    }

    if(!PyString_Check(object)) {
        python_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    self->plugin->log().warning("%s", message);

    Py_RETURN_NONE;
}

PyObject* log_object_t::error(log_object_t* self, PyObject* args) {
    PyObject* object;
    const char* message;

    if(!self->plugin) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Not initialized"
        );

        return NULL;
    }
    
    if(!PyArg_ParseTuple(args, "O:error", &object)) {
        return NULL;
    }

    if(!PyString_Check(object)) {
        python_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    self->plugin->log().error("%s", message);

    Py_RETURN_NONE;
}
