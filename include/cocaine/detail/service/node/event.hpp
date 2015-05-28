#pragma once

#include <string>

namespace cocaine { namespace app {

class event_t {
    std::string name_;

public:
    event_t(std::string name):
        name_(std::move(name))
    {}

    std::string
    name() const {
        return name_;
    }
};

}} // namespace cocaine::app
