#pragma once

#include <string>

#include <boost/optional.hpp>

class splitter_t {
public:
    std::string unparsed;

    void
    consume(const std::string& data) {
        unparsed.append(data);
    }

    boost::optional<std::string>
    next() {
        auto pos = unparsed.find('\n');
        if (pos == std::string::npos) {
            return boost::none;
        }

        auto line = unparsed.substr(0, pos);
        unparsed.erase(0, pos + 1);
        return boost::make_optional(line);
    }
};
