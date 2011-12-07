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

#ifndef COCAINE_BIRTH_CONTROL_HPP
#define COCAINE_BIRTH_CONTROL_HPP

namespace cocaine { namespace helpers {

template<class T>
class birth_control_t  {
    public:
        static uint64_t objects_alive;
        static uint64_t objects_created;

        birth_control_t() {
            ++objects_alive;
            ++objects_created;
        }

    protected:
        ~birth_control_t() {
            --objects_alive;
        }
};

template<class T>
uint64_t birth_control_t<T>::objects_alive(0);

template<class T>
uint64_t birth_control_t<T>::objects_created(0);

}}

#endif
