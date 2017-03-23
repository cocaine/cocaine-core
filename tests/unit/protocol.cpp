#include <cocaine/rpc/protocol.hpp>

#include <cocaine/idl/storage.hpp>

namespace cocaine {

static_assert(
    std::is_same<
        io::messages<io::storage_tag>::type,
        boost::mpl::list<
            io::storage::read,
            io::storage::write,
            io::storage::remove,
            io::storage::find
        >::type
    >::value,
    "`io::messages<T>::type` is broken");

static_assert(
    boost::mpl::equal<
        io::messages<io::storage_tag>::full::type,
        boost::mpl::list<
            io::storage::read,
            io::storage::write,
            io::storage::remove,
            io::storage::find,
            io::control::goaway,
            io::control::ping,
            io::control::settings,
            io::control::revoke
        >::type
    >::value,
    "`io::messages<T>::full` is broken");

} // namespace cocaine
