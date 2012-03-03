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

#include <sstream>
#include <boost/filesystem/fstream.hpp>

#include <EXTERN.h>
#include <perl.h>

#include "cocaine/app.hpp"
#include "cocaine/interfaces/module.hpp"
#include "cocaine/interfaces/plugin.hpp"
#include "cocaine/registry.hpp"

EXTERN_C void boot_DynaLoader(pTHX_ CV* cv);

EXTERN_C void xs_init(pTHX) {
    char* file = (char*)__FILE__;
    dXSUB_SYS;

    // DynaLoader is a special case
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

namespace cocaine {
namespace engine {

class perl_t: public plugin_t, public core::module_t<perl_t> {
public:
    perl_t(context_t& ctx) : plugin_t(ctx, "perl") {
        PERL_SYS_INIT3(NULL, NULL, NULL);

        my_perl = perl_alloc();
        perl_construct(my_perl);
    }

    ~perl_t() {
        perl_destruct(my_perl);
        perl_free(my_perl);
        PERL_SYS_TERM();
    }

    virtual void initialize(const app_t& app) {
        Json::Value args(app.manifest["args"]);

        if(!args.isObject()) {
            throw unrecoverable_error_t("malformed manifest");
        }

        boost::filesystem::path source(args["source"].asString());

        if(source.empty()) {
            throw unrecoverable_error_t("no code location has been specified");
        }

        if(boost::filesystem::is_directory(source)) {
            throw unrecoverable_error_t("malformed manifest, expected path to perl script, got a directory.");   
        }

        std::string source_dir;
        #if BOOST_FILESYSTEM_VERSION == 3
            source_dir = source.parent_path().string();
        #else
            source_dir = source.branch_path().string();
        #endif

        boost::filesystem::ifstream input(source);

        if(!input) {
            throw unrecoverable_error_t("unable to open " + source.string());
        }

        const char* embedding[] = {"", (char*)source.string().c_str(), "-I", (char*)source_dir.c_str()};
        perl_parse(my_perl, xs_init, 4, (char**)embedding, NULL);
        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
        log().info("%s", "running interpreter...");
        perl_run(my_perl);
    }
        
    virtual void invoke(io_t& io, const std::string& method) {
        log().info("%s", (std::string("invoking method ") + method + "...").c_str());
        std::string input;
        
        if (io.request && io.request_size > 0) {
           input = std::string((const char*)io.request, io.request_size);
        }

        PERL_SET_CONTEXT(my_perl);

        std::string result;
        const char* input_value_buff = NULL;

        // init stack pointer
        dSP;
        ENTER;
        SAVETMPS;

        // remember stack pointer
        PUSHMARK(SP);

        int call_flags = G_EVAL | G_SCALAR | G_NOARGS;

        // if there's an argument - push it on the stack
        if (!input.empty()) {
        	call_flags = G_EVAL | G_SCALAR;
            input_value_buff = input.c_str();

            XPUSHs(sv_2mortal(newSVpv(input_value_buff, 0)));
            PUTBACK;
        }

        // call function
        int ret_vals_count = call_pv(method.c_str(), call_flags);

        // refresh stack pointer
		SPAGAIN;

		// get error (if any)
		if (SvTRUE(ERRSV)) {
			STRLEN n_a;

			std::string error_msg = "perl eval error: ";
			error_msg += SvPV(ERRSV, n_a);
			throw unrecoverable_error_t(error_msg);
		}

		// pop returned value of the stack
		if (ret_vals_count > 0) {
			char* str_ptr = savepv(POPp);

			if (str_ptr) {
				result = std::string(str_ptr);
			}
		}

		// clean-up
		PUTBACK;
		FREETMPS;
		LEAVE;

		// invoke callback with resulting data
        if (!result.empty()) {
            io.push(result.data(), result.size());
        }
    }

private:
    PerlInterpreter* my_perl;
};

extern "C" {
    void initialize(core::registry_t& registry) {
        registry.install<perl_t>("perl");
    }

    __attribute__((destructor)) void finalize() {
    }
}

}}
