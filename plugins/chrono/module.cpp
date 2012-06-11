//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "recurring_timer.hpp"
#include "drifting_timer.hpp"

using namespace cocaine;
using namespace cocaine::engine::drivers;

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<recurring_timer_t>("recurring-timer");
        repository.insert<drifting_timer_t>("drifting-timer");
    }
}
