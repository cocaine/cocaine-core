#ifndef COCAINE_FORWARDS_HPP
#define COCAINE_FORWARDS_HPP

namespace cocaine {
    namespace engine {
        class backend_t;
        class deferred_t;
        class driver_t;
        
        class engine_t;
        
        enum error_code {
            request_error     = 400,
            server_error      = 500,
            application_error = 502,
            resource_error    = 503,
            timeout_error     = 504
        };
        
        class overseer_t;
    }

    namespace plugin {
        class source_t;
    }
}

#endif
