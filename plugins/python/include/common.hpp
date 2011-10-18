#ifndef COCAINE_PYTHON_COMMON_HPP
#define COCAINE_PYTHON_COMMON_HPP

#include <Python.h>

#include "cocaine/common.hpp"
#include "cocaine/helpers/track.hpp"

namespace cocaine { namespace plugin {

typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
typedef helpers::track<PyObject*, Py_DecRef> object_t;

class python_support_t {
    public:
        // Fetches and formats current Python exception as a string
        static std::string exception();

        // Unwraps the Python result object
        static Json::Value unwrap(PyObject* object);
        static PyObject* wrap(const Json::Value& value);

    private:
        python_support_t();
};

}}

#endif
