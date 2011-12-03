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
