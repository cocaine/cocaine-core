#ifndef COCAINE_PYTHON_COMMON_HPP
#define COCAINE_PYTHON_COMMON_HPP

#include <Python.h>

#include "cocaine/plugin.hpp"
#include "cocaine/helpers/track.hpp"

#include "common.hpp"

namespace cocaine { namespace plugin {

typedef helpers::track<PyObject*, Py_DecRef> object_t;

class interpreter_t {
    public:
        interpreter_t(PyThreadState** state):
            m_saved(NULL)
        {
            PyEval_AcquireLock();

            if(*state == NULL) {
                *state = Py_NewInterpreter();

                if(*state == NULL) {
                    throw std::runtime_error("failed to create a python interpreter");
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
    public source_t
{
    public:
        python_t(const std::string& args);

        virtual void invoke(
            callback_fn_t callback,
            const std::string& method, 
            const void* request,
            size_t size);

    protected:
        static void exception();
        
        virtual void respond(
            callback_fn_t callback,
            object_t& result) = 0;

    private:
        void compile(
            const std::string& path,
            const std::string& code);

    private:
        object_t m_module;
        PyThreadState* m_interpreter;
};

}}

#endif
