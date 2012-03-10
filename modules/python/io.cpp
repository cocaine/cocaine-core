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

#include "python.hpp"
#include "io.hpp"

using namespace cocaine::engine;

int python_io_t::constructor(python_io_t* self, PyObject* args, PyObject* kwargs) {    
    PyObject * py_io;

    if(!PyArg_ParseTuple(args, "O", &py_io))
        return NULL;

    self->io = static_cast<io_t*>(PyCObject_AsVoidPtr(py_io));

    Py_DECREF(py_io);
          
    return 0;
}

void python_io_t::destructor(python_io_t* self) {
    self->ob_type->tp_free(self);    
}

PyObject* python_io_t::read(python_io_t* self, PyObject* args) {
    data_container_t chunk = self->io->pull(false);

    if (!chunk.data() || !chunk.size())
        Py_RETURN_NONE;

    python_object_t string(
        PyString_FromStringAndSize(
            static_cast<const char*>(chunk.data()), 
            chunk.size()
        )
    );
    
    return string;
}

PyObject* python_io_t::write(python_io_t* self, PyObject* args) {    
    const char * message;
    size_t size;

    if(!PyArg_ParseTuple(args, "s#", &message, &size))
        return NULL;

    self->io->push(message, size);

    Py_RETURN_NONE;
}