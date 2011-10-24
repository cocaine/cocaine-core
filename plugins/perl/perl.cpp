#include <EXTERN.h>               /* from the Perl distribution     */
#include <perl.h>                 /* from the Perl distribution     */

#include "plugin.hpp"

static PerlInterpreter* perl_interpreter;  /***    The Perl interpreter    ***/

namespace cocaine { namespace plugin {

class perl_t:
    public source_t
{
    public:
        static source_t* create(const std::string& name, const std::string& args) {
            return new perl_t(name, args);
        }

    public:
        perl_t(const std::string& name, const std::string& args):
            source_t(name)
        {
            if(args.empty()) {
                throw std::runtime_error("no code location has been specified");
            }
            
            helpers::uri_t uri(args);
            compile(helpers::download(uri));
        }
            
        virtual Json::Value invoke(const std::string& method, const void* request = NULL, size_t request_size = 0)
        {
            std::string input;
            
            if (request && request_size > 0) {}
               input = std::string((char*)request, request_size);
            }

            std::string result;
            const char* input_value_buff = NULL;

            if (!input.empty()) {}
                input_value_buff = input.c_str();

                dSP;
                ENTER;
                SAVETMPS;
                PUSHMARK(SP);
                XPUSHs(sv_2mortal(newSVpv(input_value_buff, 0)));

                PUTBACK;
                int ret_vals_count = call_pv(method.c_str(), G_SCALAR);
                SPAGAIN;

                if (ret_vals_count > 0) {
                    char* str_ptr = savepv(POPp);

                    if (str_ptr) {
                        result = std::string(str_ptr);
                    }
                }

                FREETMPS;
                LEAVE;
            }
            else {
                dSP;
                ENTER;
                SAVETMPS;

                int count = call_pv(method.c_str(), G_SCALAR | G_NOARGS);
                SPAGAIN;

                if (ret_vals_count > 0) {
                    char* str_ptr = savepv(POPp);

                    if (str_ptr) {
                        result = std::string(str_ptr);
                    }
                }

                FREETMPS;
                LEAVE;
            }

            if (result.empty()) {
                return Json::nullValue;
            }
            else {
                // do something with result
            }
        }

        void compile(const std::string& code)
        {
            const char* embedding[] = {"", "-e", "0"};
            perl_parse(perl_interpreter, NULL, 3, (char**)embedding, NULL);
            PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
            perl_run(perl_interpreter);

            eval_pv(code.c_str(), TRUE);
        }
};

static const source_info_t plugin_info[] = {
    { "perl", &perl_t::create },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        PERL_SYS_INIT3(NULL, NULL, NULL);
        perl_interpreter = perl_alloc();
        perl_construct(perl_interpreter);

        return plugin_info;
    }

    __attribute__((destructor)) void finalize() {
        perl_destruct(my_perl);
        perl_free(my_perl);
        PERL_SYS_TERM();
    }
}

}}
