#pragma once

#include <string>

#include "cocaine/idl/rpc.hpp"
#include "cocaine/rpc/dispatch.hpp"

namespace cocaine {

/// The basic prototype.
///
/// It's here only, because Cocaine API wants it in actors. Does nothing, because it is always
/// replaced by a handshake dispatch for every incoming connection.
class hostess_t:
    public dispatch<io::worker_tag>
{
public:
    hostess_t(const std::string& name) :
        dispatch<io::worker_tag>(format("%s/hostess", name))
    {}
};

} // namespace cocaine
