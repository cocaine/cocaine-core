#include <EXTERN.h>               /* from the Perl distribution     */
#include <perl.h>                 /* from the Perl distribution     */

#include "cocaine/plugin.hpp"
#include "cocaine/downloads.hpp"
#include "cocaine/helpers/uri.hpp"

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

            my_perl = perl_alloc();
            PERL_SET_CONTEXT(my_perl);
            perl_construct(my_perl);

            compile(helpers::download(uri));
        }

        ~perl_t() {
            PERL_SET_CONTEXT(my_perl);
            perl_destruct(my_perl);
            perl_free(my_perl);
        }
            
        virtual Json::Value invoke(const std::string& method, const void* request = NULL, size_t request_size = 0)
        {
            std::cout << "method: " << method << std::endl;

            if (request) {
                try {
                    std::string tmp_str = std::string((char*)request, request_size);
                    std::cout << "request: " << tmp_str << std::endl;
                }
                catch (...) {
                    std::cout << "not string request" << std::endl;
                }
            }
            else {
                std::cout << "request: NULL" << std::endl;
            }
            
            std::cout << "request_size: " << request_size << std::endl;

            std::string input;
            
            if (request && request_size > 0) {
               input = std::string((char*)request, request_size);
            }

            std::string result;
            const char* input_value_buff = NULL;

            if (!input.empty()) {
                input_value_buff = input.c_str();

                PERL_SET_CONTEXT(my_perl);
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
                PERL_SET_CONTEXT(my_perl);
                dSP;
                ENTER;
                SAVETMPS;

                int ret_vals_count = call_pv(method.c_str(), G_SCALAR | G_NOARGS);
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
                Json::Value object(Json::objectValue);
                object["result"] = result;

                return object;
            }
        }

        void compile(const std::string& code)
        {
            PERL_SET_CONTEXT(my_perl);
            const char* embedding[] = {"", "-e", "0"};
            perl_parse(my_perl, NULL, 3, (char**)embedding, NULL);
            PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
            perl_run(my_perl);

            eval_pv(code.c_str(), TRUE);
        }
    
    private:
        PerlInterpreter* my_perl;  /***    The Perl interpreter    ***/
};

static const source_info_t plugin_info[] = {
    { "perl", &perl_t::create },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        PERL_SYS_INIT3(NULL, NULL, NULL);
        return plugin_info;
    }

    __attribute__((destructor)) void finalize() {
        PERL_SYS_TERM();
    }
}

}}
