/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <cocaine/logging.hpp>

#include "log.hpp"
#include "python.hpp"

using namespace cocaine::engine;

int log_object_t::constructor(log_object_t * self,
                              PyObject * args,
                              PyObject * kwargs)
{
    PyObject * builtins = PyEval_GetBuiltins();
    PyObject * sandbox = PyDict_GetItemString(builtins, "__sandbox__");
    
    if(sandbox) {
        self->sandbox = static_cast<python_t*>(PyCObject_AsVoidPtr(sandbox));
    } else {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Corrupted context"
        );

        return -1;
    }

    return 0;
}

void log_object_t::destructor(log_object_t * self) {
    self->ob_type->tp_free(self);
}

PyObject* log_object_t::debug(log_object_t * self,
                              PyObject * args)
{
    PyObject * object = NULL;
    const char * message = NULL;

    if(!self->sandbox) {
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
        tracked_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    self->sandbox->log().debug("%s", message);

    Py_RETURN_NONE;
}

PyObject* log_object_t::info(log_object_t * self,
                             PyObject * args)
{
    PyObject * object = NULL;
    const char * message = NULL;

    if(!self->sandbox) {
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
        tracked_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    self->sandbox->log().info("%s", message);

    Py_RETURN_NONE;
}

PyObject* log_object_t::warning(log_object_t * self,
                                PyObject * args)
{
    PyObject * object = NULL;
    const char * message = NULL;

    if(!self->sandbox) {
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
        tracked_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    self->sandbox->log().warning("%s", message);

    Py_RETURN_NONE;
}

PyObject* log_object_t::error(log_object_t * self,
                              PyObject * args)
{
    PyObject * object = NULL;
    const char * message = NULL;

    if(!self->sandbox) {
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
        tracked_object_t string(PyObject_Str(object));
        message = PyString_AsString(string);
    } else {
        message = PyString_AsString(object);
    }

    self->sandbox->log().error("%s", message);

    Py_RETURN_NONE;
}

PyObject* log_object_t::write(log_object_t * self,
                              PyObject * args)
{
    const char * message = NULL;

    if(!self->sandbox) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Not initialized"
        );

        return NULL;
    }

    if(!PyArg_ParseTuple(args, "s:write", &message)) {
        return NULL;
    }

    self->sandbox->log().error("%s", message);

    Py_RETURN_NONE;
}

PyObject* log_object_t::writelines(log_object_t * self,
                                   PyObject * args)
{
    PyObject * lines = NULL;

    if(!self->sandbox) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Not initialized"
        );

        return NULL;
    }

    if(!PyArg_ParseTuple(args, "O:writelines", &lines)) {
        return NULL;
    }

    tracked_object_t iterator(PyObject_GetIter(lines));
    tracked_object_t line(NULL);

    if(PyErr_Occurred()) {
        return NULL;
    }

    while(true) {
        line = PyIter_Next(iterator);

        if(!line.valid()) {
            if(!PyErr_Occurred()) {
                break;
            } else {
                return NULL;
            }
        }

        tracked_object_t argpack(PyTuple_Pack(1, *line));

        if(!write(self, argpack)) {
            return NULL;
        }
    }

    Py_RETURN_NONE;
}

PyObject* log_object_t::flush(log_object_t * self,
                              PyObject * args)
{
    Py_RETURN_NONE;
}
