/*
    Copyright (C) 2011-2012 Alexander Eliseev <admin@inkvi.com>
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

#ifndef COCAINE_PYTHON_SANDBOX_IO_HPP
#define COCAINE_PYTHON_SANDBOX_IO_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include "Python.h"

namespace cocaine { namespace engine {

class io_t;

class python_io_t {
    public:
        PyObject_HEAD

        static int constructor(python_io_t * self,
                               PyObject * args,
                               PyObject * kwargs);

        static void destructor(python_io_t * self);

        static PyObject* read(python_io_t * self,
                              PyObject * args,
                              PyObject * kwargs);

        static PyObject* write(python_io_t * self,
                               PyObject * args);

        // static PyObject* delegate(python_io_t * self, 
        //                           PyObject * args,
        //                           PyObject * kwargs);

        // WSGI requirements.
        static PyObject* readline(python_io_t * self,
                                  PyObject * args,
                                  PyObject * kwargs);

        static PyObject* readlines(python_io_t * self,
                                   PyObject * args,
                                   PyObject * kwargs);
        
        static PyObject* iter_next(python_io_t * it);

    public:
        io_t * io;

        blob_t request;
        off_t offset;
};

}}
#endif
