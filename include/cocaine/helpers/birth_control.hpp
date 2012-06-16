/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#ifndef COCAINE_HELPERS_BIRTH_CONTROL_HPP
#define COCAINE_HELPERS_BIRTH_CONTROL_HPP

namespace cocaine { namespace helpers {

template<class T>
class birth_control  {
    public:
        static uint64_t objects_alive;
        static uint64_t objects_created;

        birth_control() {
            ++objects_alive;
            ++objects_created;
        }

    protected:
        ~birth_control() {
            --objects_alive;
        }
};

template<class T>
uint64_t birth_control<T>::objects_alive(0);

template<class T>
uint64_t birth_control<T>::objects_created(0);

}

using helpers::birth_control;

}

#endif
