#ifndef YAPPI_PYTHON_HPP
#define YAPPI_PYTHON_HPP

#include <Python.h>

#include "plugin.hpp"
#include "track.hpp"

namespace yappi { namespace plugin {

// Format: python:///path/to/file.py/callable?arg1=val1&arg2=...
class python_t: public source_t {
    public:
        python_t(const std::string& uri);

        void instantiate(const std::string& code,
                         const std::string& name, 
                         const dict_t& parameters);
        virtual dict_t fetch();

        std::string exception();

    protected:
        static char identity[];

        typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
        typedef helpers::track<PyObject*, Py_DecRef> object_t;
        
        object_t m_module, m_object;
};

}}

#endif
