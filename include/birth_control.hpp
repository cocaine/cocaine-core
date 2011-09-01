#ifndef YAPPI_BIRTH_CONTROL_HPP
#define YAPPI_BIRTH_CONTROL_HPP

#include <boost/thread.hpp>
#include <boost/utility/in_place_factory.hpp>

namespace yappi { namespace helpers {

template<class T>
class factory_t {
    public:
        static boost::thread_specific_ptr<T>& open(const config_t& config) {
            if(!instance.get()) {
                instance.reset(new T(config));
            }

            return instance;
        }

    private:
        static boost::thread_specific_ptr<T> instance;
};

template<class T>
boost::thread_specific_ptr<T> factory_t<T>::instance(NULL);

template<class T>
class birth_control_t  {
    public:
        static unsigned int objects_alive;
        static unsigned int objects_created;

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
unsigned int birth_control_t<T>::objects_alive(0);

template<class T>
unsigned int birth_control_t<T>::objects_created(0);

}}

#endif
