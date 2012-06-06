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

#include "cocaine/app.hpp"

#include "cocaine/engine.hpp"

using namespace cocaine;
using namespace cocaine::engine;

app_t::app_t(context_t& context, const std::string& name):
	m_engine(new engine_t(context, name))
{ }

app_t::~app_t() {
    m_engine.reset();
}

void app_t::start() {
	m_engine->start();
}

void app_t::stop() {
	m_engine->stop();
}

Json::Value app_t::info() const {
	return m_engine->info();
}

void app_t::enqueue(const boost::shared_ptr<job_t>& job) {
	m_engine->enqueue(job);
}
