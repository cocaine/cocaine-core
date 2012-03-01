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

#include "log.hpp"

#if PY_VERSION_HEX > 0x02070000
    #include <pycapsule.h>
#endif

using namespace cocaine::engine;

PyObject* log_object_t::__new__(PyTypeObject* type, PyObject* args, PyObject* kwargs) {
    log_object_t* self = reinterpret_cast<log_object_t*>(type->tp_alloc(type, 0));

    if(self != NULL) {

    }

    return reinterpret_cast<PyObject*>(self);
}

int log_object_t::__init__(log_object_t* self, PyObject* args, PyObject* kwargs) {
    return 0;
}

void log_object_t::__del__(log_object_t* self) {
    self->ob_type->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject* log_object_t::debug(log_object_t* self, PyObject* args, PyObject* kwargs) {
    Py_RETURN_NONE;
}

PyObject* log_object_t::info(log_object_t* self, PyObject* args, PyObject* kwargs) {
    Py_RETURN_NONE;
}

PyObject* log_object_t::warning(log_object_t* self, PyObject* args, PyObject* kwargs) {
    Py_RETURN_NONE;
}

PyObject* log_object_t::error(log_object_t* self, PyObject* args, PyObject* kwargs) {
    Py_RETURN_NONE;
}