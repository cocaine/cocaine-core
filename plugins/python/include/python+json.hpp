#ifndef COCAINE_PYTHON_JSON_HPP
#define COCAINE_PYTHON_JSON_HPP

#include "python+raw.hpp"

namespace cocaine { namespace plugin {

class json_python_t:
    public raw_python_t
{
    public:
        static source_t* create(const std::string& args);

    public:
        json_python_t(const std::string& args):
            raw_python_t(args)
        { }

    private:
        virtual void respond(callback_fn_t callback, object_t& result);
        Json::Value convert(PyObject* result);
};

}}

#endif
