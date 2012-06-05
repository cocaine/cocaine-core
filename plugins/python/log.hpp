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

#ifndef COCAINE_PYTHON_SANDBOX_LOG_HPP
#define COCAINE_PYTHON_SANDBOX_LOG_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include "Python.h"

namespace cocaine { namespace engine {

class python_t;

class log_object_t {
    public:
        PyObject_HEAD

        static int constructor(log_object_t * self,
                               PyObject * args,
                               PyObject * kwargs);

        static void destructor(log_object_t * self);

        static PyObject* debug(log_object_t * self,
                               PyObject * args);

        static PyObject* info(log_object_t * self,
                              PyObject * args);

        static PyObject* warning(log_object_t * self,
                                 PyObject * args);

        static PyObject* error(log_object_t * self,
                               PyObject * args);

        // WSGI requirements.
        static PyObject* write(log_object_t * self,
                               PyObject * args);

        static PyObject* writelines(log_object_t * self,
                                    PyObject * args);

        static PyObject* flush(log_object_t * self,
                               PyObject * args);

    public:
        python_t * sandbox;
};

}}

#endif
