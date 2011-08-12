#ifndef YAPPI_BIRTH_CONTROL_HPP
#define YAPPI_BIRTH_CONTROL_HPP

namespace yappi { namespace helpers {

class single_t {
    public:
        inline static bool overflow(uint64_t population) {
            return population > 1;
        }

    private:
        single_t();
};

template<uint64_t population_limit>
class limited_t {
    public:
        inline static bool overflow(uint64_t population) {
            return population > population_limit;
        }

    private:
        limited_t();
};

class unlimited_t {
    public:
        inline static bool overflow(uint64_t population) {
            return false;
        }

    private:
        unlimited_t();
};

template<class T, class Limit = unlimited_t>
class birth_control_t  {
    public:
        static uint64_t objects_alive;
        static uint64_t objects_created;

        birth_control_t() {
            ++objects_alive;
            ++objects_created;

            if(Limit::overflow(objects_alive)) {
                --objects_alive;
                throw std::runtime_error("object population limit reached");
            }
        }

    protected:
        ~birth_control_t() {
            --objects_alive;
        }
};

template<class T, class Limit>
uint64_t birth_control_t<T, Limit>::objects_alive(0);

template<class T, class Limit>
uint64_t birth_control_t<T, Limit>::objects_created(0);

}}

#endif
