//
// Copyright (C) 2011 Rim Zaidullin <creator@bash.org.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "cocaine/plugin.hpp"
#include "cocaine/downloads.hpp"
#include "cocaine/helpers/uri.hpp"

#include <EXTERN.h>               /* from the Perl distribution     */
#include <perl.h>                 /* from the Perl distribution     */

namespace cocaine { namespace plugin {

class perl_t:
    public source_t
{
    public:
        static source_t* create(const std::string& args) {
            return new perl_t(args);
        }

    public:
        perl_t(const std::string& args) {
            if(args.empty()) {
                throw unrecoverable_error_t("no code location has been specified");
            }
            
            helpers::uri_t uri(args);

            my_perl = perl_alloc();
            perl_construct(my_perl);

            compile(helpers::download(uri));
        }

        ~perl_t() {
            perl_destruct(my_perl);
            perl_free(my_perl);
        }
            
        virtual void invoke(
            callback_fn_t callback,
            const std::string& method,
            const void* request,
            size_t size)
        {
            std::string input;
            
            if (request && size > 0) {
               input = std::string((char*)request, size);
            }

            std::string result;
            const char* input_value_buff = NULL;

            if (!input.empty()) {
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
                PERL_SET_CONTEXT(my_perl);
                dSP;
                ENTER;
                SAVETMPS;

                PUSHMARK(SP);				
                PUTBACK;

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

            if (!result.empty()) {
                callback(result.data(), result.size());
            }
        }

        void compile(const std::string& code)
        {
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
