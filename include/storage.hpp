#ifndef YAPPI_STORAGE_HPP
#define YAPPI_STORAGE_HPP

#include "detail/void.hpp"
#include "detail/files.hpp"
#include "detail/mongo.hpp"
// #include "detail/eblobs.hpp"

namespace yappi { namespace storage {
    typedef backends::mongo_storage_t storage_t;
}}

#endif
