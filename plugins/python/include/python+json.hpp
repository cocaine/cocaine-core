#ifndef COCAINE_PYTHON_JSON_HPP
#define COCAINE_PYTHON_JSON_HPP

#include "common.hpp"

namespace cocaine { namespace plugin {

class python_json_t:
    public python_t
{
    public:
        static source_t* create(const std::string& args);

        python_json_t(const std::string& args):
            python_t(args)
        { }

    private:
        virtual void respond(
            callback_fn_t callback,
            object_t& result);

        Json::Value convert(PyObject* result);
};

}}

#endif
