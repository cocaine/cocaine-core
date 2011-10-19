#ifndef COCAINE_STORAGE_HPP
#define COCAINE_STORAGE_HPP

#include "cocaine/storages/abstract.hpp"

namespace cocaine { namespace storage {

class storage_t {
    public:
        static boost::shared_ptr<abstract_storage_t> instance();

    private:
        storage_t();

        static boost::shared_ptr<abstract_storage_t> g_object;
};

}}

#endif
