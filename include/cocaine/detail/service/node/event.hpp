#pragma once

#include <chrono>
#include <string>

namespace cocaine { namespace app {

class event_t {
public:
    const std::string name;
    const std::chrono::high_resolution_clock::time_point birthstamp;

    event_t(std::string name):
        name(std::move(name)),
        birthstamp(std::chrono::high_resolution_clock::now())
    {}
};

}} // namespace cocaine::app
