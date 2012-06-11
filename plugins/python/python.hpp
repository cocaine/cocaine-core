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

#ifndef COCAINE_PYTHON_SANDBOX_HPP
#define COCAINE_PYTHON_SANDBOX_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include <Python.h>

#include "cocaine/interfaces/sandbox.hpp"

#include "cocaine/helpers/json.hpp"
#include "cocaine/helpers/track.hpp"

namespace cocaine { namespace engine {

typedef track_t<PyObject*, Py_DecRef> tracked_object_t;

/*
class interpreter_t {
    public:
        interpreter_t(PyThreadState ** state):
            m_saved(NULL)
        {
            PyEval_AcquireLock();

            if(*state == NULL) {
                *state = Py_NewInterpreter();

                if(*state == NULL) {
                    throw unrecoverable_error("unable to create a python interpreter");
                }
            }

            m_saved = PyThreadState_Swap(*state);
        }

        ~interpreter_t() {
            PyThreadState_Swap(m_saved);
            PyEval_ReleaseLock();
        }

    private:
        PyThreadState * m_saved;
};

class thread_state_t {
    public:
        thread_state_t() {
            m_saved = PyGILState_Ensure();
        }

        ~thread_state_t() {
            PyGILState_Release(m_saved);
        }

    private:
        PyGILState_STATE m_saved;
};
*/

class thread_lock_t {
    public:
        thread_lock_t(PyThreadState * thread) {
            BOOST_ASSERT(thread != 0);
            PyEval_RestoreThread(thread);
        }

        ~thread_lock_t() {
            PyEval_SaveThread();
        }
};

class python_t:
    public sandbox_t
{
    public:
        typedef sandbox_t category_type;

    public:
        python_t(context_t& context,
                 const manifest_t& manifest);
        
        virtual ~python_t();
        
        virtual void invoke(const std::string& method,
                            io_t& io);

    public:
        const logging::logger_t& log() const {
            return *m_log;
        }
    
    public:
        static PyObject* manifest(PyObject * self,
                                  PyObject * args);
        
        static PyObject* wrap(const Json::Value& value);
        
        static std::string exception();

    private:
        boost::shared_ptr<logging::logger_t> m_log;
        
        PyObject * m_python_module;
        tracked_object_t m_python_manifest;
        
        PyThreadState * m_thread_state;
};

}}

#endif
