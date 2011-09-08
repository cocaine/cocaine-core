#ifndef YAPPI_STORAGE_HPP
#define YAPPI_STORAGE_HPP

#include "common.hpp"
#include "storages/abstract.hpp"

namespace yappi { namespace storage {

class storage_t {
    public:
        static boost::shared_ptr<abstract_storage_t> instance();

    private:
        static boost::shared_ptr<abstract_storage_t> object;
};

}}

#endif
