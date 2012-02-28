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

#include <cstdio>

#include "cocaine/logging.hpp"

#include "cocaine/context.hpp"

using namespace cocaine::logging;

emitter_t::emitter_t(context_t& context, const std::string& source):
	m_sink(context.sink()),
	m_source(source + ": ")
{ }

void emitter_t::debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    emit(logging::debug, format, args);
    va_end(args);
}

void emitter_t::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    emit(logging::info, format, args);
    va_end(args);
}

void emitter_t::warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    emit(logging::warning, format, args);
    va_end(args);
}

void emitter_t::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    emit(logging::error, format, args);
    va_end(args);
}

void emitter_t::emit(priorities priority, const char* format, va_list args) {
    char buffer[256];
    
    vsprintf(buffer, format, args);            
    m_sink.emit(priority, m_source + buffer);
}
