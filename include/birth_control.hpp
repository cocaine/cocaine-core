#ifndef YAPPI_BIRTH_CONTROL_HPP
#define YAPPI_BIRTH_CONTROL_HPP

namespace yappi { namespace helpers {

template< class T,
          template<typename> class PointerType = std::auto_ptr>
class factory_t {
    public:
        static T* instance() {
            if(!object.get()) {
                object.reset(new T());
            }

            return object.get();
        }

    private:
        static PointerType<T> object;
};

template< class T, template<typename> class PointerType>
PointerType<T> factory_t<T, PointerType>::object;

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
