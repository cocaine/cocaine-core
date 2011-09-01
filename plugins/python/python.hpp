#ifndef YAPPI_PYTHON_HPP
#define YAPPI_PYTHON_HPP

#include <Python.h>

#include "plugin.hpp"
#include "track.hpp"

namespace yappi { namespace plugin {

class python_t: public source_t {
    public:
        // The source protocol implementation
        python_t(const std::string& uri);

        // Instantiates the iterable object from the supplied code
        void compile(const std::string& code,
                     const std::string& name, 
                     const dict_t& parameters);

        virtual dict_t invoke();
        virtual float reschedule();
        
        virtual uint64_t capabilities() const;

        // Fetches and formats current Python exception as a string
        std::string exception();

    public:
        static char identity[];

    private:
        typedef helpers::track<PyGILState_STATE, PyGILState_Release> thread_state_t;
        typedef helpers::track<PyObject*, Py_DecRef> object_t;
        
        object_t m_module, m_object;
};

}}

#endif
