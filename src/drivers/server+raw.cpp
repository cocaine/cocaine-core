#include "cocaine/drivers/server+raw.hpp"

using namespace cocaine::engine::drivers;

raw_server_t::raw_server_t(engine_t* engine, const std::string& method, const Json::Value& args):
    driver_t(engine, method),
    m_port(args.get("port", 0).asUInt()),
    m_backlog(args.get("backlog", 100).asUInt()),
#if defined SOCK_CLOEXEC && defined SOCK_NONBLOCK
    m_socket(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_TCP))
#else
    m_socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
#endif
{
    if(m_socket < 0) {
        throw std::runtime_error("unable to create a socket");
    }

    if(!m_port) {
        throw std::runtime_error("no port has been specified for the '" + m_method + "' task");
    }
    
    // Enable address reusing
    int reuse = 1;

    if(::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        throw std::runtime_error("unable to configure the socket (address reuse)");
    }

#if !defined SOCK_NONBLOCK && defined FD_NONBLOCK
    // Enable nonblocking I/O
    if(::fcntl(m_socket, F_SETFD, FD_NONBLOCK) < 0) {
        throw std::runtime_error("unable to configure the socket (non-blocking)");
    }
#endif

#if !defined SOCK_CLOEXEC && defined FD_CLOEXEC
    // Close socket on fork
    if(::fcntl(m_socket, F_SETFD, FD_CLOEXEC) < 0) {
        throw std::runtime_error("unable to configure the socket (close-on-exec)");
    }
#endif

    sockaddr_in address;

    memset(&address, 0, sizeof(sockaddr_in));

    address.sin_family = AF_INET;
    address.sin_port = ::htons(m_port);

    // TODO: Allow to bind on a specific interface/address
    address.sin_addr.s_addr = INADDR_ANY;

    if(::bind(m_socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error("unable to bind the socket");
    }

    if(::listen(m_socket, m_backlog) < 0) {
        throw std::runtime_error("unable to start listening on the socket");
    }

    m_watcher.set(this);
    m_watcher.start(m_socket, ev::READ);
}

raw_server_t::~raw_server_t() {
    pause();

    ::shutdown(m_socket, SHUT_RDWR);
    ::close(m_socket);
}

void raw_server_t::pause() {
    m_watcher.stop();
}

void raw_server_t::resume() {
    m_watcher.start();
}

void raw_server_t::operator()(ev::io&, int) {
#if defined SOCK_CLOEXEC && defined SOCK_NONBLOCK
    int fd = ::accept4(m_socket, NULL, 0, SOCK_CLOEXEC | SOCK_NONBLOCK);
#else
    int fd = ::accept(m_socket, NULL, 0);
    
    ::fcntl(m_socket, F_SETFD, FD_CLOEXEC);
    ::fcntl(m_socket, F_SETFD, FD_NONBLOCK);
#endif

    process(fd);
}

