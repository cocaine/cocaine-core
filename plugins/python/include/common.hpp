#ifndef COCAINE_PYTHON_COMMON_HPP
#define COCAINE_PYTHON_COMMON_HPP

#include <Python.h>

#include "cocaine/plugin.hpp"
#include "cocaine/helpers/track.hpp"

#include "common.hpp"

namespace cocaine { namespace plugin {

typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
typedef helpers::track<PyObject*, Py_DecRef> object_t;

class interpreter_t {
    public:
        interpreter_t(PyThreadState** enable) {
            PyEval_AcquireLock();

            if(*enable == NULL) {
                *enable = Py_NewInterpreter();
            }

            m_disabled = PyThreadState_Swap(*enable);
        }

        ~interpreter_t() {
            PyThreadState_Swap(m_disabled);
            PyEval_ReleaseLock();
        }

    private:
        PyThreadState *m_disabled;
};

class python_t:
    public source_t
{
    public:
        python_t(const std::string& args);

        virtual ~python_t();

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
        void compile(const std::string& code);

    private:
        static char identity[];
        
        object_t m_module;
        PyThreadState* m_interpreter;
};

}}

#endif
