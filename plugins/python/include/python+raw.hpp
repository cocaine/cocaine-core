#ifndef COCAINE_PYTHON_RAW_HPP
#define COCAINE_PYTHON_RAW_HPP

#include <Python.h>

#include "cocaine/plugin.hpp"
#include "cocaine/helpers/track.hpp"

namespace cocaine { namespace plugin {

typedef helpers::track<PyObject*, Py_DecRef> object_t;

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

class raw_python_t:
    public source_t
{
    public:
        static source_t* create(const std::string& args);
    
    public:    
        raw_python_t(const std::string& args);

        virtual void invoke(callback_fn_t callback,
                            const std::string& method, 
                            const void* request,
                            size_t size);

    protected:
        virtual void respond(callback_fn_t callback, object_t& result);
        static std::string exception();

    private:
        void compile(const std::string& path, const std::string& code);

    protected:
        object_t m_module;
};

}}

#endif
