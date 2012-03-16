//
// Copyright (C) 2011-2012 Alexander Eliseev <admin@inkvi.com>
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

#ifndef COCAINE_PYTHON_PLUGIN_IO_HPP
#define COCAINE_PYTHON_PLUGIN_IO_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include "Python.h"

namespace cocaine { namespace engine {

class io_t;

class python_io_t {
    public:
        PyObject_HEAD

        static int constructor(python_io_t * self, PyObject * args, PyObject * kwargs);
        static void destructor(python_io_t * self);

        static PyObject* read(python_io_t * self, PyObject * args, PyObject * kwargs);
        static PyObject* write(python_io_t * self, PyObject * args);
        
        // WSGI requirements.
        static PyObject* readline(python_io_t * self, PyObject * args, PyObject * kwargs);
        static PyObject* readlines(python_io_t * self, PyObject * args, PyObject * kwargs);
        static PyObject* iter_next(python_io_t * it);

    public:
        io_t * io;
};

}}
#endif
