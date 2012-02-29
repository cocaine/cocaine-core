//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/object.hpp"

using namespace cocaine;

object_t::object_t(context_t& context, const std::string& identity):
    m_context(context),
    m_log(context, identity)
{
    log().debug("constructing");
}

object_t::~object_t() {
    log().debug("destructing");
}
