#ifndef COCAINE_PYTHON_HPP
#define COCAINE_PYTHON_HPP

#include <Python.h>

#include "cocaine/plugin.hpp"
#include "cocaine/helpers/track.hpp"

namespace cocaine { namespace plugin {

typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
typedef helpers::track<PyObject*, Py_DecRef> object_t;

class python_t:
    public source_t
{
    public:
        // The source protocol implementation
        python_t(const std::string& uri);

        // Source protocol
        virtual Json::Value invoke(const std::string& callable, 
            const void* request = NULL, size_t request_length = 0);

    private:
        // Instantiates the iterable object from the supplied code
        void compile(const std::string& code);

        // Fetches and formats current Python exception as a string
        static std::string exception();

        // Unwraps the Python result object
        static Json::Value unwrap(PyObject* object);

    public:
        static char identity[];

    private:
        object_t m_module;
};

}}

#endif
