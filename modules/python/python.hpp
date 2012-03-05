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

#ifndef COCAINE_PYTHON_PLUGIN_HPP
#define COCAINE_PYTHON_PLUGIN_HPP

// NOTE: These are being redefined in Python.h
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include <Python.h>

#include "cocaine/helpers/track.hpp"
#include "cocaine/interfaces/plugin.hpp"

namespace cocaine { namespace engine {

typedef helpers::track<PyObject*, Py_DecRef> python_object_t;

/*
class interpreter_t {
    public:
        interpreter_t(PyThreadState** state):
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
        PyThreadState* m_saved;
};
*/

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

class python_t:
    public plugin_t
{
    public:
        python_t(context_t& ctx);
        
        virtual void initialize(const engine::app_t& app);
        virtual void invoke(io_t& io, const std::string& method);

    public:
        static PyObject* manifest(PyObject* self, PyObject*);
        static PyObject* wrap(const Json::Value& value);
        
        static std::string exception();

    private:
        void respond(io_t& io, python_object_t& result);

    private:
        PyObject* m_python_module;
        python_object_t m_manifest;
};

}}

#endif
