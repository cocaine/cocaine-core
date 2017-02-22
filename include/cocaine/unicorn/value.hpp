/*
* 2015+ Copyright (c) Anton Matveenko <antmat@yandex-team.ru>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#pragma once

#include "cocaine/common.hpp"
#include "cocaine/dynamic.hpp"

namespace cocaine {
namespace unicorn {

using value_t = dynamic_t;
using version_t = long long;

static constexpr version_t min_version = -2;
static constexpr version_t not_existing_version = -1;

class versioned_value_t {
    struct {
        value_t value;
        version_t version;
    } data;

public:
    versioned_value_t(value_t value, version_t version);

    auto
    value() const -> const value_t& {
        return data.value;
    }

    auto
    version() const -> version_t {
        return data.version;
    }

    auto
    exists() const noexcept -> bool {
        return version() != not_existing_version;
    }
};

}  // namespace unicorn
}  // namespace cocaine
